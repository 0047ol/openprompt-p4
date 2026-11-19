#include "wifi_settings.h"
#include "control_session.h"
#include "bluetooth_control.h"
#include "freertos/task.h"
#include "firmware_update.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "lwip/sockets.h"
#include "network_address.h"
#include "factory_reset.h"
#include "mdns.h"
#include "web_assets.h"

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t s_apply_lock;
static esp_timer_handle_t s_retry, s_connect_deadline;
static char s_ssid[33], s_ip[16];
static bool s_enabled, s_connected, s_ap_enabled = true;
static bool s_bluetooth_guide;
static bool s_lan_verified;
static wifi_portal_mode_t s_portal_mode;
static bool s_wifi_stopped;
static bool s_switching;
static tp_network_address_t s_network = {.version=1,.ap="192.168.4.1",.mask="255.255.255.0",.hostname="teleprompter"};
static tp_network_address_t s_network_active;
static bool s_network_pending, s_network_failed;
static bool s_mdns_ready, s_mdns_initialized;
static unsigned s_attempts;
static tp_guide_peers_t s_guide_peers;
static struct {uint8_t mac[6];esp_ip4_addr_t ip;} s_ap_addresses[TP_GUIDE_PEER_CAPACITY];
void wifi_settings_device_mac(char *out,size_t capacity)
{
    uint8_t mac[6];esp_netif_t *netif=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if(netif&&esp_netif_get_mac(netif,mac)==ESP_OK)
        snprintf(out,capacity,"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    else if(capacity)out[0]=0;
}
static int s_reason;
static void schedule_connect(void);
static esp_err_t set_hotspot(bool enabled);
static void apply_mdns_name(const char *hostname)
{
    s_mdns_ready=false;
    if(!hostname[0]){if(s_mdns_initialized)mdns_free();s_mdns_initialized=false;return;}
    esp_err_t err=ESP_OK;
    if(!s_mdns_initialized){err=mdns_init();s_mdns_initialized=err==ESP_OK;}
    if(err==ESP_OK)err=mdns_hostname_set(hostname);
    if(err==ESP_OK){
        if(mdns_service_exists("_http","_tcp",NULL))
            (void)mdns_service_remove("_http","_tcp");
        err=mdns_service_add(NULL,"_http","_tcp",80,NULL,0);
    }
    s_mdns_ready=err==ESP_OK;
    if(err!=ESP_OK)ESP_LOGW("tp_wifi","mDNS unavailable: %s",esp_err_to_name(err));
}
void wifi_settings_hotspot_address(char *out, size_t capacity)
{
    esp_netif_ip_info_t info;
    esp_netif_t *ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if(ap&&esp_netif_get_ip_info(ap,&info)==ESP_OK&&info.ip.addr)
        snprintf(out,capacity,IPSTR,IP2STR(&info.ip));
    else snprintf(out,capacity,"192.168.4.1");
}

static esp_err_t apply_network_address(const tp_network_address_t *config)
{
    esp_netif_t *ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_t *sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if(!ap||!sta||!tp_network_address_valid(config))return ESP_ERR_INVALID_ARG;
    esp_err_t err=esp_netif_dhcpc_stop(sta);
    if(err!=ESP_OK&&err!=ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)return err;
    esp_netif_ip_info_t info={0};
    if(config->manual){
        esp_netif_str_to_ip4(config->ip,&info.ip);
        esp_netif_str_to_ip4(config->mask,&info.netmask);
        esp_netif_str_to_ip4(config->gateway,&info.gw);
    }
    err=esp_netif_set_ip_info(sta,&info);
    if(err!=ESP_OK)return err;
    if(config->manual){
        esp_netif_dns_info_t dns={0};dns.ip.type=ESP_IPADDR_TYPE_V4;
        esp_netif_str_to_ip4(config->dns,&dns.ip.u_addr.ip4);
        err=esp_netif_set_dns_info(sta,ESP_NETIF_DNS_MAIN,&dns);
    }else err=esp_netif_dhcpc_start(sta);
    if(err!=ESP_OK&&err!=ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)return err;

    esp_netif_ip_info_t old={0};
    esp_netif_get_ip_info(ap,&old);
    esp_netif_str_to_ip4(config->ap,&info.ip);
    esp_netif_str_to_ip4("255.255.255.0",&info.netmask);
    info.gw=info.ip;
    if(old.ip.addr==info.ip.addr)return ESP_OK;
    esp_netif_dhcp_status_t status;
    err=esp_netif_dhcps_get_status(ap,&status);if(err!=ESP_OK)return err;
    if(status==ESP_NETIF_DHCP_STARTED){
        /* Force clients to acquire an address in the new subnet. */
        (void)esp_wifi_deauth_sta(0);
        err=esp_netif_dhcps_stop(ap);if(err!=ESP_OK)return err;
    }
    err=esp_netif_set_ip_info(ap,&info);
    if(err==ESP_OK&&status==ESP_NETIF_DHCP_STARTED)err=esp_netif_dhcps_start(ap);
    return err;
}

static bool network_json(const cJSON *json,tp_network_address_t *config)
{
    if(!cJSON_IsObject(json))return false;
    /* Merge under s_apply_lock so simultaneous HTTP/BLE edits cannot replace
     * unrelated fields with a stale snapshot. */
    *config=s_network;
    bool supplied=false,address_changed=false;
    const cJSON *mdns=cJSON_GetObjectItemCaseSensitive(json,"mdnsEnabled");
    if(mdns){
        if(!cJSON_IsBool(mdns))return false;
        config->mdns_enabled=cJSON_IsTrue(mdns);supplied=true;
    }
    const cJSON *manual=cJSON_GetObjectItemCaseSensitive(json,"staticIp");
    if(manual){
        if(!cJSON_IsBool(manual))return false;
        config->manual=cJSON_IsTrue(manual);supplied=true;address_changed=true;
    }
    const cJSON *hostname=cJSON_GetObjectItemCaseSensitive(json,"hostname");
    if(hostname){
        if(!cJSON_IsString(hostname)||strlen(hostname->valuestring)>=sizeof(config->hostname))return false;
        snprintf(config->hostname,sizeof(config->hostname),"%s",hostname->valuestring);supplied=true;
    }
    if(cJSON_GetObjectItemCaseSensitive(json,"ip")&&!manual)config->manual=true;
    if(config->manual&&!s_network.manual){
        /* A DHCP lease is the only source for implicit gateway/mask/DNS.
         * Never guess a router or silently reuse data from another network. */
        esp_netif_t *sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_ip_info_t info={0};esp_netif_dns_info_t dns={0};
        if(!s_connected||!sta||esp_netif_get_ip_info(sta,&info)!=ESP_OK||!info.ip.addr||
           !info.netmask.addr||!info.gw.addr||esp_netif_get_dns_info(sta,ESP_NETIF_DNS_MAIN,&dns)!=ESP_OK||
           dns.ip.type!=ESP_IPADDR_TYPE_V4||!dns.ip.u_addr.ip4.addr)return false;
        snprintf(config->ip,sizeof(config->ip),IPSTR,IP2STR(&info.ip));
        snprintf(config->mask,sizeof(config->mask),IPSTR,IP2STR(&info.netmask));
        snprintf(config->gateway,sizeof(config->gateway),IPSTR,IP2STR(&info.gw));
        snprintf(config->dns,sizeof(config->dns),IPSTR,IP2STR(&dns.ip.u_addr.ip4));
    }
    const char *keys[]={"apIp","ip","netmask","gateway","dns"};
    char *fields[]={config->ap,config->ip,config->mask,config->gateway,config->dns};
    for(unsigned i=0;i<5;i++){
        const cJSON *v=cJSON_GetObjectItemCaseSensitive(json,keys[i]);
        if(!v)continue;
        if(!cJSON_IsString(v)||strlen(v->valuestring)>=16)return false;
        snprintf(fields[i],16,"%s",v->valuestring);supplied=true;address_changed=true;
    }
    if(!supplied||!tp_network_address_valid(config))return false;
    if(address_changed&&!config->manual&&s_connected){
        esp_netif_ip_info_t current;
        esp_netif_t *sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if(sta&&esp_netif_get_ip_info(sta,&current)==ESP_OK&&current.ip.addr&&current.netmask.addr){
            uint32_t ap=0;
            if(!tp_ipv4(config->ap,&ap))return false;
            uint32_t common=ntohl(current.netmask.addr)&UINT32_C(0xffffff00);
            if((ap&common)==(ntohl(current.ip.addr)&common))return false;
        }
    }
    return true;
}
esp_err_t wifi_settings_refresh_ap_name(const char *name)
{
    if(!s_apply_lock||firmware_update_in_progress())return ESP_ERR_INVALID_STATE;
    if(xSemaphoreTake(s_apply_lock,0)!=pdTRUE)return ESP_ERR_INVALID_STATE;
    wifi_config_t config={0};wifi_sta_list_t clients={0};
    esp_err_t err=ESP_ERR_INVALID_STATE;
    if(!s_switching&&!s_wifi_stopped&&esp_wifi_ap_get_sta_list(&clients)==ESP_OK&&clients.num==0){
        err=esp_wifi_get_config(WIFI_IF_AP,&config);
        if(err==ESP_OK){
            memset(config.ap.ssid,0,sizeof(config.ap.ssid));config.ap.ssid_len=strlen(name);
            memcpy(config.ap.ssid,name,config.ap.ssid_len);err=esp_wifi_set_config(WIFI_IF_AP,&config);
        }
    }
    memset(&config,0,sizeof(config));xSemaphoreGive(s_apply_lock);return err;
}
void wifi_settings_portal_mode(wifi_portal_mode_t mode)
{
    portENTER_CRITICAL(&s_lock);s_portal_mode=mode;portEXIT_CRITICAL(&s_lock);
}
void wifi_settings_tick(void){}
void wifi_settings_portal_status(const char **phase,int *remaining_ms)
{
    *phase="coexisting";*remaining_ms=0;
}

void wifi_settings_confirm_lan(httpd_req_t *req, const char *client)
{
    size_t length = client ? strlen(client) : 0;
    if (length < 8 || length > 32) return;
    for (size_t i = 0; i < length; ++i)
        if (!((client[i] >= '0' && client[i] <= '9') ||
              (client[i] >= 'a' && client[i] <= 'f'))) return;
    control_session_console_http(req);
    struct sockaddr_storage local = {0};
    socklen_t size = sizeof(local);
    if (getsockname(httpd_req_to_sockfd(req), (struct sockaddr *)&local, &size)) return;
    struct in_addr address;
    if (local.ss_family == AF_INET) {
        address = ((struct sockaddr_in *)&local)->sin_addr;
#if CONFIG_LWIP_IPV6
    } else if (local.ss_family == AF_INET6) {
        const unsigned char *bytes = (const unsigned char *)&((struct sockaddr_in6 *)&local)->sin6_addr;
        const unsigned char prefix[12] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff};
        if (memcmp(bytes, prefix, sizeof(prefix))) return;
        memcpy(&address, bytes + 12, sizeof(address));
#endif
    } else return;
    char ip[16];
    if (!inet_ntop(AF_INET, &address, ip, sizeof(ip))) return;
    portENTER_CRITICAL(&s_lock);
    bool confirmed = s_enabled && s_connected && !s_lan_verified && !strcmp(ip, s_ip);
    if (confirmed) s_lan_verified = true;
    portEXIT_CRITICAL(&s_lock);
}
static void connection_expired(void *arg)
{
    (void)arg;
    portENTER_CRITICAL(&s_lock);
    bool retry=s_enabled&&!s_connected;
    if(retry)s_attempts=5;
    portEXIT_CRITICAL(&s_lock);
    if(retry){esp_wifi_disconnect();schedule_connect();}
}

static void retry_connect(void *arg)
{
    (void)arg;
    portENTER_CRITICAL(&s_lock);
    bool connect = s_enabled && !s_connected;
    bool absent=connect&&s_attempts>=5&&s_reason==WIFI_REASON_NO_AP_FOUND;
    portEXIT_CRITICAL(&s_lock);
    /* Scanning for an unavailable saved router shares C6's radio with AP
     * transfers. After the initial attempts, leave active hotspot control
     * and downloads alone; explicit Wi-Fi configuration resets attempts. */
    if(absent&&(control_session_hotspot_controller()||web_assets_busy()||firmware_update_in_progress())){
        esp_timer_start_once(s_retry,30000000);return;
    }
    portENTER_CRITICAL(&s_lock);
    connect=s_enabled&&!s_connected;
    if (connect && s_attempts < 5) ++s_attempts;
    portEXIT_CRITICAL(&s_lock);
    if (connect && esp_wifi_connect() != ESP_OK) {
        schedule_connect();
    } else if(connect) {
        esp_timer_stop(s_connect_deadline);
        esp_timer_start_once(s_connect_deadline,15000000);
    }
}

static void schedule_connect(void)
{
    portENTER_CRITICAL(&s_lock);
    bool slow_retry = s_attempts >= 5;
    portEXIT_CRITICAL(&s_lock);
    esp_timer_stop(s_retry);
    esp_timer_start_once(s_retry, slow_retry ? 30000000 : 1000000);
}

static esp_err_t set_hotspot(bool enabled)
{
    portENTER_CRITICAL(&s_lock);
    bool unchanged = s_ap_enabled == enabled && !enabled;
    portEXIT_CRITICAL(&s_lock);
    if (unchanged) return ESP_OK;
    esp_err_t err = esp_wifi_set_mode(enabled ? WIFI_MODE_APSTA : WIFI_MODE_STA);
    if (err == ESP_OK) {
        /* Reapply the app's latency policy after the remote controller changes
         * mode; its STA-only default must not silently restore modem sleep. */
        esp_err_t power_err = esp_wifi_set_ps(WIFI_PS_NONE);
        ESP_LOGI("tp_wifi", "mode=%s power-save=none: %s", enabled ? "APSTA" : "STA", esp_err_to_name(power_err));
        portENTER_CRITICAL(&s_lock);
        s_ap_enabled = enabled;
        portEXIT_CRITICAL(&s_lock);
    }
    return err;
}

static void station_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if(base==IP_EVENT&&id==IP_EVENT_AP_STAIPASSIGNED&&data){
        const ip_event_assigned_ip_to_client_t *event=data;
        if(event->esp_netif==esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")){
            portENTER_CRITICAL(&s_lock);
            unsigned slot=TP_GUIDE_PEER_CAPACITY;
            for(unsigned i=0;i<TP_GUIDE_PEER_CAPACITY;i++){
                if(!memcmp(s_ap_addresses[i].mac,event->mac,6)){slot=i;break;}
                if(!s_ap_addresses[i].ip.addr)slot=i;
            }
            if(slot<TP_GUIDE_PEER_CAPACITY){memcpy(s_ap_addresses[slot].mac,event->mac,6);s_ap_addresses[slot].ip=event->ip;}
            portEXIT_CRITICAL(&s_lock);
        }
        return;
    }
    if(base==WIFI_EVENT&&(id==WIFI_EVENT_AP_STACONNECTED||id==WIFI_EVENT_AP_STADISCONNECTED||id==WIFI_EVENT_AP_STOP)){
        portENTER_CRITICAL(&s_lock);
        if(id==WIFI_EVENT_AP_STOP){memset(&s_guide_peers,0,sizeof(s_guide_peers));memset(s_ap_addresses,0,sizeof(s_ap_addresses));}
        else if(data){
            const uint8_t *mac=id==WIFI_EVENT_AP_STACONNECTED?
                ((wifi_event_ap_staconnected_t *)data)->mac:((wifi_event_ap_stadisconnected_t *)data)->mac;
            tp_guide_peer_update(&s_guide_peers,mac,id==WIFI_EVENT_AP_STACONNECTED);
        }
        portEXIT_CRITICAL(&s_lock);
        if(id==WIFI_EVENT_AP_STOP)control_session_network_disconnected(true);
        else if(id==WIFI_EVENT_AP_STADISCONNECTED&&data){
            esp_netif_pair_mac_ip_t pair={0};
            memcpy(pair.mac,((wifi_event_ap_stadisconnected_t *)data)->mac,6);
            portENTER_CRITICAL(&s_lock);
            for(unsigned i=0;i<TP_GUIDE_PEER_CAPACITY;i++)if(!memcmp(s_ap_addresses[i].mac,pair.mac,6)){
                pair.ip=s_ap_addresses[i].ip;memset(&s_ap_addresses[i],0,sizeof(s_ap_addresses[i]));break;
            }
            portEXIT_CRITICAL(&s_lock);
            esp_netif_t *ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
            if(!pair.ip.addr&&ap)(void)esp_netif_dhcps_get_clients_by_mac(ap,1,&pair);
            if(pair.ip.addr){
                char address[16];snprintf(address,sizeof(address),IPSTR,IP2STR(&pair.ip));
                control_session_release(address);
            }
        }
        return;
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        char address[16];
        snprintf(address, sizeof(address), IPSTR, IP2STR(&event->ip_info.ip));
        esp_timer_stop(s_retry);
        esp_timer_stop(s_connect_deadline);
        portENTER_CRITICAL(&s_lock);
        bool enabled = s_enabled;
        if (strcmp(s_ip, address)) s_lan_verified = false;
        if (enabled) memcpy(s_ip, address, sizeof(s_ip));
        s_connected = enabled;
        s_reason = 0;
        s_attempts = 0;
        portEXIT_CRITICAL(&s_lock);
        if(enabled){control_session_set_mode(TP_LINK_LAN);control_session_lan_ready();}
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = data;
        esp_timer_stop(s_connect_deadline);
        portENTER_CRITICAL(&s_lock);
        s_connected = false;
        s_lan_verified = false;
        s_ip[0] = 0;
        s_reason = event->reason;
        bool retry = s_enabled;
        portEXIT_CRITICAL(&s_lock);
        control_session_network_disconnected(false);
        if (retry) schedule_connect();
    }
}

static bool valid_credentials(const char *ssid, const char *password)
{
    size_t name_len = strlen(ssid), pass_len = strlen(password);
    if (!name_len || name_len > 32) return false;
    for (size_t i = 0; i < name_len; ++i)
        if ((unsigned char)ssid[i] < 32) return false;
    if (!pass_len) return true;
    if (pass_len < 8 || pass_len > 64) return false;
    for (size_t i = 0; i < pass_len; ++i) {
        unsigned char c = password[i];
        if (c < 32 || c > 126) return false;
        if (pass_len == 64 && !((c >= '0' && c <= '9') ||
                               (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

static esp_err_t configure_station(const char *ssid, const char *password)
{
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, ssid, strlen(ssid));
    memcpy(config.sta.password, password, strlen(password));
    /* FAST_SCAN stops at the first matching SSID, even a distant extender.
     * Compare all matching APs before choosing; keep BSSID/channel unpinned. */
    config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    config.sta.threshold.authmode = password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    return esp_wifi_set_config(WIFI_IF_STA, &config);
}

esp_err_t wifi_settings_init(void)
{
    uint8_t mac[6];
    if(esp_wifi_get_mac(WIFI_IF_AP,mac)==ESP_OK)
        snprintf(s_network.hostname,sizeof(s_network.hostname),"teleprompter-%02x%02x",mac[4],mac[5]);
    s_apply_lock=xSemaphoreCreateMutex();
    if(!s_apply_lock)return ESP_ERR_NO_MEM;
    const esp_timer_create_args_t timer = {.callback = retry_connect, .name = "wifi_retry"};
    esp_err_t err = esp_timer_create(&timer, &s_retry);
    if (err != ESP_OK) return err;
    const esp_timer_create_args_t deadline_timer={.callback=connection_expired,.name="wifi_deadline"};
    err=esp_timer_create(&deadline_timer,&s_connect_deadline);if(err!=ESP_OK)return err;
    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, station_event, NULL);
    if (err != ESP_OK) return err;
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, station_event, NULL);
    if (err != ESP_OK) return err;
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, station_event, NULL);
    if (err != ESP_OK) return err;
    nvs_handle_t handle;
    if (nvs_open("tp_wifi", NVS_READONLY, &handle) == ESP_OK) {
        char password[65] = {0};
        size_t name_size = sizeof(s_ssid), pass_size = sizeof(password);
        bool loaded = nvs_get_str(handle, "ssid", s_ssid, &name_size) == ESP_OK &&
                      nvs_get_str(handle, "password", password, &pass_size) == ESP_OK &&
                      valid_credentials(s_ssid, password);
        tp_network_address_t saved;size_t saved_size=sizeof(saved);
        if(nvs_get_blob(handle,"address",&saved,&saved_size)==ESP_OK&&saved_size==sizeof(saved)&&
           memchr(saved.ap,0,16)&&memchr(saved.ip,0,16)&&memchr(saved.mask,0,16)&&
           memchr(saved.gateway,0,16)&&memchr(saved.dns,0,16)&&memchr(saved.hostname,0,64)&&tp_network_address_valid(&saved))s_network=saved;
        nvs_close(handle);
        /* Retain credentials for an explicit Wi-Fi choice in controller settings.
         * Each boot still starts with the device hotspot available. */
        memset(password, 0, sizeof(password));
        if (!loaded) s_ssid[0] = 0;
    }
    err=apply_network_address(&s_network);
    if(err!=ESP_OK){
        ESP_LOGW("tp_wifi","Address configuration failed; using defaults: %s",esp_err_to_name(err));
        s_network=(tp_network_address_t){.version=1,.ap="192.168.4.1",.mask="255.255.255.0",.hostname="teleprompter"};
        err=apply_network_address(&s_network);
    }
    s_network_active=s_network;
    return err;
}

void wifi_settings_start(void)
{
    esp_err_t err=wifi_settings_use_hotspot();
    ESP_LOGI("tp_wifi","Startup hotspot: %s",esp_err_to_name(err));
    if(err==ESP_OK&&s_ssid[0])wifi_settings_switch_mode("lan");
    apply_mdns_name(s_network.mdns_enabled?s_network.hostname:"");
}

void wifi_settings_address(char *out, size_t capacity)
{
    char ap[16];wifi_settings_hotspot_address(ap,sizeof(ap));
    portENTER_CRITICAL(&s_lock);
    snprintf(out, capacity, "%s", s_connected ? s_ip : ap);
    portEXIT_CRITICAL(&s_lock);
}

void wifi_settings_guide_snapshot(tp_guide_input_t *out)
{
    char ap[16];wifi_settings_hotspot_address(ap,sizeof(ap));
    portENTER_CRITICAL(&s_lock);
    *out=(tp_guide_input_t){.hotspot=s_ap_enabled,.wifi_enabled=s_enabled,.wifi_connected=s_connected,
        .wifi_failed=s_enabled&&!s_connected&&s_attempts>=5,.bluetooth_selected=s_bluetooth_guide,
        .hotspot_clients=tp_guide_peer_count(&s_guide_peers)};
    snprintf(out->ssid,sizeof(out->ssid),"%s",s_ssid);
    snprintf(out->address,sizeof(out->address),"%s",s_connected?s_ip:ap);
    snprintf(out->hotspot_address,sizeof(out->hotspot_address),"%s",ap);
    if(s_mdns_ready&&s_network_active.mdns_enabled&&s_network_active.hostname[0])
        snprintf(out->local_domain,sizeof(out->local_domain),"%s.local",s_network_active.hostname);
    portEXIT_CRITICAL(&s_lock);
    out->controller=control_session_connected();
    if(!control_session_network_connected()&&control_session_bluetooth_connected())out->bluetooth_selected=true;
    out->portal_open=control_session_portal_open();
}

void wifi_settings_show_bluetooth(bool enabled)
{
    portENTER_CRITICAL(&s_lock);s_bluetooth_guide=enabled;portEXIT_CRITICAL(&s_lock);
}

bool wifi_settings_bluetooth_guide(void)
{
    portENTER_CRITICAL(&s_lock);bool enabled=s_bluetooth_guide;portEXIT_CRITICAL(&s_lock);
    return enabled;
}
bool wifi_settings_hotspot_enabled(void)
{
    portENTER_CRITICAL(&s_lock);bool enabled=s_ap_enabled;portEXIT_CRITICAL(&s_lock);
    return enabled;
}

esp_err_t wifi_settings_use_hotspot(void)
{
    control_session_set_mode(TP_LINK_HOTSPOT);
    bluetooth_control_diagnostic_pause(false);
    wifi_settings_show_bluetooth(false);
    /* Transport choices last for this boot; router credentials remain saved. */
    portENTER_CRITICAL(&s_lock);
    s_enabled=false;s_connected=false;s_lan_verified=false;s_ip[0]=0;s_attempts=0;
    s_portal_mode=WIFI_PORTAL_NONE;
    portEXIT_CRITICAL(&s_lock);
    esp_timer_stop(s_retry);esp_timer_stop(s_connect_deadline);
    esp_err_t err=set_hotspot(true);
    esp_wifi_disconnect();
    if(err==ESP_OK)err=esp_wifi_set_mode(WIFI_MODE_APSTA);
    control_session_network_disconnected(false);
    return err;
}

static void escape_json(const char *input, char *output)
{
    while (*input) {
        unsigned char c = *input++;
        if (c == '"' || c == '\\') { *output++ = '\\'; *output++ = c; }
        else if (c < 32) { snprintf(output, 7, "\\u%04x", c); output += 6; }
        else *output++ = c;
    }
    *output = 0;
}

static cJSON *wifi_snapshot(void)
{
    char ssid[33], ip[16], ap[16];
    tp_network_address_t network;
    wifi_settings_hotspot_address(ap,sizeof(ap));
    bool enabled, connected, ap_enabled, lan_verified;
    unsigned attempts;
    int reason;
    portENTER_CRITICAL(&s_lock);
    memcpy(ssid, s_ssid, sizeof(ssid));
    memcpy(ip, s_ip, sizeof(ip));
    enabled = s_enabled; connected = s_connected; attempts = s_attempts; reason = s_reason;
    ap_enabled = s_ap_enabled;
    lan_verified = s_lan_verified;
    network=s_network;
    portEXIT_CRITICAL(&s_lock);
    char escaped[199], body[640];
    const char *close_state;int close_remaining;
    wifi_settings_portal_status(&close_state,&close_remaining);
    escape_json(ssid, escaped);
    const char *state = connected ? "connected" : !enabled ? "unconfigured" :
                        attempts >= 5 ? "failed" : "connecting";
    snprintf(body, sizeof(body), "{\"configured\":%s,\"connected\":%s,\"ssid\":\"%s\","
             "\"ip\":\"%s\",\"apIp\":\"%s\",\"apEnabled\":%s,\"lanVerified\":%s,\"status\":\"%s\",\"reason\":%d,"
             "\"apCloseState\":\"%s\",\"apCloseRemainingMs\":%d}",
             enabled ? "true" : "false", connected ? "true" : "false", escaped, ip,ap,
             ap_enabled ? "true" : "false", lan_verified ? "true" : "false", state, reason,close_state,close_remaining);
    cJSON *result=cJSON_Parse(body);
    if(result){
        char mac[18];wifi_settings_device_mac(mac,sizeof(mac));
        cJSON_AddStringToObject(result,"mac",mac);
        cJSON_AddStringToObject(result,"connectionMode",control_session_mode_name());
        cJSON_AddBoolToObject(result,"switching",s_switching);
        cJSON_AddNumberToObject(result,"controllerLimit",0);
        cJSON_AddBoolToObject(result,"connectionsCoexist",true);
        cJSON_AddNumberToObject(result,"networkControllerLimit",0);
        cJSON_AddNumberToObject(result,"bluetoothControllerLimit",0);
        cJSON_AddBoolToObject(result,"bluetoothAlwaysAvailable",true);
        if(s_mdns_ready&&s_network_active.mdns_enabled&&s_network_active.hostname[0]){
            char domain[70];snprintf(domain,sizeof(domain),"%s.local",s_network_active.hostname);
            cJSON_AddStringToObject(result,"localDomain",domain);
        }
        cJSON *n=cJSON_AddObjectToObject(result,"network");
        if(n){
            cJSON_AddBoolToObject(n,"supported",true);
            cJSON_AddStringToObject(n,"apIp",network.ap);
            cJSON_AddBoolToObject(n,"staticIp",network.manual);
            cJSON_AddStringToObject(n,"ip",network.ip);
            cJSON_AddStringToObject(n,"netmask",network.mask);
            cJSON_AddStringToObject(n,"gateway",network.gateway);
            cJSON_AddStringToObject(n,"dns",network.dns);
            cJSON_AddStringToObject(n,"hostname",network.hostname);
            cJSON_AddBoolToObject(n,"mdnsEnabled",network.mdns_enabled);
            cJSON_AddBoolToObject(n,"pending",s_network_pending);
            cJSON_AddBoolToObject(n,"applyFailed",s_network_failed);
            cJSON_AddBoolToObject(n,"mdnsAvailable",s_mdns_ready);
        }
        esp_netif_t *sta=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        esp_netif_ip_info_t info;
        if(connected&&sta&&esp_netif_get_ip_info(sta,&info)==ESP_OK){
            cJSON *lease=cJSON_AddObjectToObject(result,"lease");char text[16];
            if(lease){
                snprintf(text,sizeof(text),IPSTR,IP2STR(&info.netmask));cJSON_AddStringToObject(lease,"netmask",text);
                snprintf(text,sizeof(text),IPSTR,IP2STR(&info.gw));cJSON_AddStringToObject(lease,"gateway",text);
                esp_netif_dns_info_t dns;
                if(esp_netif_get_dns_info(sta,ESP_NETIF_DNS_MAIN,&dns)==ESP_OK&&dns.ip.type==ESP_IPADDR_TYPE_V4){
                    snprintf(text,sizeof(text),IPSTR,IP2STR(&dns.ip.u_addr.ip4));cJSON_AddStringToObject(lease,"dns",text);
                }
            }
        }
    }
    return result;
}

static esp_err_t wifi_get(httpd_req_t *req)
{
    if(!control_session_network_available(req)){
        if(!control_session_http(req))return ESP_OK;
    }
    cJSON *state=wifi_snapshot();
    if(state)cJSON_AddStringToObject(state,"accessMode",control_session_http_hotspot(req)?"hotspot":"lan");
    char *body=state?cJSON_PrintUnformatted(state):NULL;
    cJSON_Delete(state);
    if(!body)return ESP_ERR_NO_MEM;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err=httpd_resp_sendstr(req, body);
    free(body);return err;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool form_value(const char *body, const char *key, char *out, size_t capacity)
{
    if(!capacity)return false;
    size_t key_length=strlen(key);
    const char *encoded=NULL,*end=NULL;
    for(const char *part=body;*part;){
        const char *stop=strchr(part,'&');if(!stop)stop=part+strlen(part);
        if((size_t)(stop-part)>key_length&&part[key_length]=='='&&!memcmp(part,key,key_length)){
            encoded=part+key_length+1;end=stop;break;
        }
        part=*stop?stop+1:stop;
    }
    if(!encoded)return false;
    size_t n = 0;
    for (size_t i = 0; encoded+i<end; ++i) {
        unsigned char c = encoded[i];
        if (c == '%') {
            if (end-(encoded+i)<3) return false;
            int hi = hex_digit(encoded[i + 1]), lo = hex_digit(encoded[i + 2]);
            if (hi < 0 || lo < 0) return false;
            c = (hi << 4) | lo; i += 2;
        } else if (c == '+') c = ' ';
        if (!c || n + 1 >= capacity) return false;
        out[n++] = c;
    }
    out[n] = 0;
    return true;
}

static esp_err_t wifi_apply_credentials(const char *ssid,const char *password,bool forget,const cJSON *json,bool *invalid_network);

static esp_err_t wifi_post(httpd_req_t *req)
{
    /* A stale guide cannot reconfigure an occupied device. A real network
     * controller still configures Wi-Fi normally after the guide closes. */
    if(!control_session_network_controller(req)&&!control_session_portal_http(req))return ESP_OK;
    if(!control_session_http(req))return ESP_OK;
    if(firmware_update_in_progress()||factory_reset_pending()){
        httpd_resp_set_status(req,"409 Conflict");
        return httpd_resp_sendstr(req,"firmware_busy");
    }
    if (!req->content_len || req->content_len > 2048)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid Wi-Fi settings");
    char body[2049], ssid[33] = {0}, password[65] = {0}, action[16] = {0}, network_text[768];
    size_t received = 0;
    while (received < req->content_len) {
        int n = httpd_req_recv(req, body + received, req->content_len - received);
        if (n <= 0) return ESP_FAIL;
        received += n;
    }
    body[received] = 0;
    if (strlen(body) != received)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid form data");
    bool forget = form_value(body, "action", action, sizeof(action)) && !strcmp(action, "forget");
    if(!strcmp(action,"mode")){
        char mode[16]={0};
        esp_err_t err=form_value(body,"mode",mode,sizeof(mode))?wifi_settings_switch_mode(mode):ESP_ERR_INVALID_ARG;
        if(err!=ESP_OK)return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid or busy connection mode");
        return wifi_get(req);
    }
    cJSON *json=NULL;
    bool has_network=form_value(body,"network",network_text,sizeof(network_text));
    if(has_network){
        json=cJSON_Parse(network_text);
        if(!cJSON_IsObject(json)){cJSON_Delete(json);return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"invalid_network_settings");}
    }else if(strstr(body,"network="))return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"invalid_network_settings");
    bool credentials=form_value(body,"ssid",ssid,sizeof(ssid));
    if(!forget&&((credentials&&(!form_value(body,"password",password,sizeof(password))||!valid_credentials(ssid,password)))||(!credentials&&!json))){
        cJSON_Delete(json);return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"invalid_wifi_settings");
    }
    bool invalid_network=false;
    esp_err_t err=wifi_apply_credentials(forget?"":credentials?ssid:NULL,password,forget,json,&invalid_network);
    cJSON_Delete(json);
    memset(password,0,sizeof(password));memset(body,0,sizeof(body));
    if(invalid_network)return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"invalid_network_settings");
    if(err!=ESP_OK)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot apply Wi-Fi settings");
    return wifi_get(req);
}

static esp_err_t wifi_apply_credentials_locked(const char *ssid,const char *password,bool forget,const tp_network_address_t *network)
{
    if(firmware_update_in_progress()||factory_reset_pending())return ESP_ERR_INVALID_STATE;
    if(s_switching)return ESP_ERR_INVALID_STATE;
    if(!forget&&ssid&&!valid_credentials(ssid,password))return ESP_ERR_INVALID_ARG;
    if(network&&!tp_network_address_valid(network))return ESP_ERR_INVALID_ARG;
    bool domain_only=false;
    if(network&&!ssid&&!forget&&!s_network_pending){
        tp_network_address_t previous=s_network;
        snprintf(previous.hostname,sizeof(previous.hostname),"%s",network->hostname);
        previous.mdns_enabled=network->mdns_enabled;
        domain_only=!memcmp(&previous,network,sizeof(previous));
        if(domain_only&&!strcmp(s_network.hostname,network->hostname)&&s_network.mdns_enabled==network->mdns_enabled)return ESP_OK;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open("tp_wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    if (forget) {
        err=nvs_erase_key(handle,"ssid");if(err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;
        if(err==ESP_OK){err=nvs_erase_key(handle,"password");if(err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;}
    }else if(ssid){
        err = nvs_set_str(handle, "ssid", ssid);
        if (err == ESP_OK) err = nvs_set_str(handle, "password", password);
    }
    if(err==ESP_OK&&network)err=nvs_set_blob(handle,"address",network,sizeof(*network));
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) return err;

    portENTER_CRITICAL(&s_lock);
    if(ssid)snprintf(s_ssid,sizeof(s_ssid),"%s",ssid);
    if(network){s_network=*network;s_network_pending=memcmp(network,&s_network_active,sizeof(*network))!=0;s_network_failed=false;}
    bool use_lan=!forget&&(ssid||s_enabled);
    portEXIT_CRITICAL(&s_lock);
    if(domain_only){
        apply_mdns_name(network->mdns_enabled?network->hostname:"");
        portENTER_CRITICAL(&s_lock);
        snprintf(s_network_active.hostname,sizeof(s_network_active.hostname),"%s",network->hostname);
        s_network_active.mdns_enabled=network->mdns_enabled;
        s_network_pending=false;
        portEXIT_CRITICAL(&s_lock);
        return ESP_OK;
    }
    return wifi_settings_switch_mode(use_lan?"lan":"hotspot");
}

static esp_err_t wifi_apply_credentials(const char *ssid,const char *password,bool forget,const cJSON *json,bool *invalid_network)
{
    if(!s_apply_lock||xSemaphoreTake(s_apply_lock,0)!=pdTRUE)return ESP_ERR_TIMEOUT;
    tp_network_address_t network;
    bool valid=!json||network_json(json,&network);
    if(invalid_network)*invalid_network=!valid;
    esp_err_t err=valid?wifi_apply_credentials_locked(ssid,password,forget,json?&network:NULL):ESP_ERR_INVALID_ARG;
    xSemaphoreGive(s_apply_lock);return err;
}

cJSON *wifi_settings_request(const cJSON *request, tp_request_source_t source)
{
    const cJSON *action=cJSON_GetObjectItemCaseSensitive(request,"action");
    const char *operation=cJSON_IsString(action)?action->valuestring:"status";
    esp_err_t err=ESP_OK;
    bool invalid_network=false;
    bool address_forbidden=false;
    if(!strcmp(operation,"mode")){
        const cJSON *mode=cJSON_GetObjectItemCaseSensitive(request,"mode");
        err=cJSON_IsString(mode)?wifi_settings_switch_mode(mode->valuestring):ESP_ERR_INVALID_ARG;
    }else if(!strcmp(operation,"save")){
        const cJSON *ssid=cJSON_GetObjectItemCaseSensitive(request,"ssid");
        const cJSON *password=cJSON_GetObjectItemCaseSensitive(request,"password");
        const cJSON *json=cJSON_GetObjectItemCaseSensitive(request,"network");
        /* Reject the whole write before parsing/applying anything. BLE may
         * manage credentials, but address/domain writes require HTTP. */
        if(json&&source!=TP_REQUEST_HTTP){address_forbidden=true;err=ESP_ERR_NOT_SUPPORTED;}
        else if(ssid&&(!cJSON_IsString(ssid)||!cJSON_IsString(password)))err=ESP_ERR_INVALID_ARG;
        else if(!ssid&&!json)err=ESP_ERR_INVALID_ARG;
        else err=wifi_apply_credentials(ssid?ssid->valuestring:NULL,ssid?password->valuestring:"",false,json,&invalid_network);
    }else if(!strcmp(operation,"forget"))err=wifi_apply_credentials("","",true,NULL,NULL);
    else if(strcmp(operation,"status"))err=ESP_ERR_INVALID_ARG;
    cJSON *result=err==ESP_OK?wifi_snapshot():cJSON_CreateObject();
    if(result){
        cJSON_AddBoolToObject(result,"ok",err==ESP_OK);
        if(err!=ESP_OK)cJSON_AddStringToObject(result,"error",address_forbidden?"network_address_forbidden":invalid_network?"invalid_network_settings":err==ESP_ERR_INVALID_ARG?"invalid_wifi_settings":err==ESP_ERR_INVALID_STATE?"firmware_busy":"wifi_settings_failed");
    }
    return result;
}

esp_err_t wifi_settings_register(httpd_handle_t server)
{
    const httpd_uri_t routes[] = {
        {.uri = "/api/wifi", .method = HTTP_GET, .handler = wifi_get},
        {.uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_post},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        esp_err_t err = httpd_register_uri_handler(server, &routes[i]);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

static void switch_worker(void *argument)
{
    tp_link_mode_t target=(tp_link_mode_t)(uintptr_t)argument;
    /* Let the HTTP/BLE save response leave before changing its address. */
    vTaskDelay(pdMS_TO_TICKS(s_network_pending?1000:100));
    /* An update begun during the handoff keeps its working transport. */
    while(firmware_update_in_progress())vTaskDelay(pdMS_TO_TICKS(1000));
    esp_timer_stop(s_retry);esp_timer_stop(s_connect_deadline);
    portENTER_CRITICAL(&s_lock);
    s_enabled=false;s_connected=false;s_lan_verified=false;s_ip[0]=0;s_attempts=0;
    portEXIT_CRITICAL(&s_lock);
    control_session_set_mode(target);
    esp_err_t err=ESP_OK;
    {
        bluetooth_control_diagnostic_pause(false);
        err=esp_wifi_set_mode(WIFI_MODE_APSTA);
        if(err==ESP_OK&&s_wifi_stopped){err=esp_wifi_start();if(err==ESP_OK)s_wifi_stopped=false;}
        if(err==ESP_OK){s_ap_enabled=true;esp_wifi_set_ps(WIFI_PS_NONE);}
        if(err==ESP_OK&&s_network_pending){
            esp_wifi_disconnect();
            tp_network_address_t next=s_network,previous=s_network_active;
            err=apply_network_address(&next);
            if(err==ESP_OK){
                s_network_active=next;
                apply_mdns_name(next.mdns_enabled?next.hostname:"");
            }else{
                ESP_LOGE("tp_wifi","Address update failed; restoring previous addresses: %s",esp_err_to_name(err));
                err=apply_network_address(&previous);
                nvs_handle_t handle;
                if(nvs_open("tp_wifi",NVS_READWRITE,&handle)==ESP_OK){
                    if(nvs_set_blob(handle,"address",&previous,sizeof(previous))==ESP_OK)(void)nvs_commit(handle);
                    nvs_close(handle);
                }
                portENTER_CRITICAL(&s_lock);s_network=previous;s_network_failed=true;portEXIT_CRITICAL(&s_lock);
            }
            portENTER_CRITICAL(&s_lock);s_network_pending=false;portEXIT_CRITICAL(&s_lock);
        }
        if(err==ESP_OK&&target==TP_LINK_HOTSPOT)err=wifi_settings_use_hotspot();
        else if(err==ESP_OK){
            char ssid[33]={0},password[65]={0};nvs_handle_t handle;
            if(nvs_open("tp_wifi",NVS_READONLY,&handle)==ESP_OK){
                size_t a=sizeof(ssid),b=sizeof(password);
                err=nvs_get_str(handle,"ssid",ssid,&a);
                if(err==ESP_OK)err=nvs_get_str(handle,"password",password,&b);
                nvs_close(handle);
            }else err=ESP_ERR_NOT_FOUND;
            esp_wifi_disconnect();
            if(err==ESP_OK)err=configure_station(ssid,password);
            memset(password,0,sizeof(password));
            if(err==ESP_OK){
                portENTER_CRITICAL(&s_lock);s_enabled=true;s_portal_mode=WIFI_PORTAL_WIFI;snprintf(s_ssid,sizeof(s_ssid),"%s",ssid);portEXIT_CRITICAL(&s_lock);
                wifi_settings_show_bluetooth(false);schedule_connect();
            }
        }
    }
    if(err!=ESP_OK){
        ESP_LOGE("tp_wifi","Transport switch failed: %s; restoring hotspot",esp_err_to_name(err));
        esp_wifi_set_mode(WIFI_MODE_APSTA);if(s_wifi_stopped&&esp_wifi_start()==ESP_OK)s_wifi_stopped=false;
        wifi_settings_use_hotspot();
    }
    bluetooth_control_diagnostic_pause(false);
    portENTER_CRITICAL(&s_lock);s_switching=false;portEXIT_CRITICAL(&s_lock);
    vTaskDelete(NULL);
}
esp_err_t wifi_settings_switch_mode(const char *name)
{
    tp_link_mode_t target;
    if(!strcmp(name,"hotspot"))target=TP_LINK_HOTSPOT;
    else if(!strcmp(name,"lan"))target=TP_LINK_LAN;
    else if(!strcmp(name,"bluetooth")){
        /* Compatibility for older pages: showing a Bluetooth guide must not
         * disconnect the network controller or reset its selected route. */
        if(firmware_update_in_progress()||factory_reset_pending())return ESP_ERR_INVALID_STATE;
        return ESP_OK;
    }
    else return ESP_ERR_INVALID_ARG;
    if(firmware_update_in_progress()||factory_reset_pending())return ESP_ERR_INVALID_STATE;
    if(target==TP_LINK_LAN&&!s_ssid[0])return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&s_lock);
    bool busy=s_switching;if(!busy)s_switching=true;
    portEXIT_CRITICAL(&s_lock);
    if(busy)return ESP_ERR_INVALID_STATE;
    if(xTaskCreate(switch_worker,"tp_link",4096,(void *)(uintptr_t)target,4,NULL)!=pdPASS){
        portENTER_CRITICAL(&s_lock);s_switching=false;portEXIT_CRITICAL(&s_lock);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
