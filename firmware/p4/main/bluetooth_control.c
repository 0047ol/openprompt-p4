#include "bluetooth_control.h"
#include "control_session.h"
#include "app_control.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_hosted.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_att.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "prompter_state.h"
#include "wifi_settings.h"
#include "device_settings.h"
#include "script_library.h"
#include "firmware_update.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <stdatomic.h>

static const char *TAG = "tp_ble";
/* UUIDs are little endian in NimBLE: 7d2a0001-4b7c-4d40-9e22-2f0b8d0c3a11. */
#define TP_UUID(n) BLE_UUID128_INIT(0x11,0x3a,0x0c,0x8d,0x0b,0x2f,0x22,0x9e,\
                                  0x40,0x4d,0x7c,0x4b,n,0x00,0x2a,0x7d)
static const ble_uuid128_t s_service = TP_UUID(1);
static const ble_uuid128_t s_status = TP_UUID(2);
static const ble_uuid128_t s_control = TP_UUID(3);
static const ble_uuid128_t s_script = TP_UUID(4);
static const ble_uuid128_t s_settings = TP_UUID(5);
static const ble_uuid128_t s_library = TP_UUID(6);
typedef struct {
    bool used,settings_active,upload_active;
    uint16_t connection;
    int64_t last_access;
    char *library_upload,*library_response;
    size_t library_expected,library_received,library_offset;
    char settings_upload[513];
    size_t settings_expected,settings_received;
} ble_session_t;
static ble_session_t s_sessions[CONFIG_BT_NIMBLE_MAX_CONNECTIONS],*s_current;
static uint8_t s_address_type;
static bool s_started;
#define s_library_upload (s_current->library_upload)
#define s_library_response (s_current->library_response)
#define s_library_expected (s_current->library_expected)
#define s_library_received (s_current->library_received)
#define s_library_offset (s_current->library_offset)
#define s_settings_upload (s_current->settings_upload)
#define s_settings_expected (s_current->settings_expected)
#define s_settings_received (s_current->settings_received)
#define s_settings_active (s_current->settings_active)
#define s_upload_active (s_current->upload_active)
#define s_last_access (s_current->last_access)
static ble_session_t *find_session(uint16_t connection)
{for(unsigned i=0;i<CONFIG_BT_NIMBLE_MAX_CONNECTIONS;i++)if(s_sessions[i].used&&s_sessions[i].connection==connection)return &s_sessions[i];return NULL;}
static unsigned connection_count(void)
{unsigned count=0;for(unsigned i=0;i<CONFIG_BT_NIMBLE_MAX_CONNECTIONS;i++)count+=s_sessions[i].used;return count;}
static void clear_session(ble_session_t *session)
{
    if(!session||!session->used)return;
    control_session_ble_release(session->connection);
    free(session->library_upload);free(session->library_response);memset(session,0,sizeof(*session));
    device_settings_ble_count(connection_count());
}
static struct ble_npl_callout s_connection_watch;
static struct ble_npl_callout s_advertising_watch;
static int64_t s_fast_advertising_until;
static bool s_fast_advertising;
static struct ble_npl_event s_diagnostic_event;
static atomic_bool s_diagnostic_paused;
static atomic_bool s_diagnostic_slow_advertising;
static struct ble_npl_event s_name_event;
static portMUX_TYPE s_name_lock=portMUX_INITIALIZER_UNLOCKED;
static char s_requested_name[30];

/* Each peer owns its partial requests and read cursor; one central cannot
 * overwrite another central's upload or steal its response. */
static void connection_watch(struct ble_npl_event *event)
{
    (void)event;
    for(unsigned i=0;i<CONFIG_BT_NIMBLE_MAX_CONNECTIONS;i++){
        ble_session_t *p=&s_sessions[i];
        if(p->used&&!firmware_update_in_progress()&&esp_timer_get_time()-p->last_access>INT64_C(45000000))
            ble_gap_terminate(p->connection,BLE_ERR_REM_USER_CONN_TERM);
    }
    if(connection_count())ble_npl_callout_reset(&s_connection_watch,ble_npl_time_ms_to_ticks32(5000));
}

static unsigned le16(const uint8_t *p) { return p[0] | ((unsigned)p[1] << 8); }

static int access_value(uint16_t connection, uint16_t attribute,
                        struct ble_gatt_access_ctxt *ctx, void *arg)
{
    (void)attribute;
    s_current=find_session(connection);
    if(!s_current||!control_session_ble_peer(connection,false))return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
    s_last_access=esp_timer_get_time();
    unsigned kind = (unsigned)(uintptr_t)arg;
    if(kind==6&&ctx->op==BLE_GATT_ACCESS_OP_READ_CHR){
        if(!s_library_response)return BLE_ATT_ERR_UNLIKELY;
        size_t total=strlen(s_library_response),offset=s_library_offset;
        if(offset>total)return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        uint8_t data[512];size_t count=total-offset;if(count>504)count=504;
        for(int i=0;i<4;i++){data[i]=(total>>(8*i))&255;data[i+4]=(offset>>(8*i))&255;}
        memcpy(data+8,s_library_response+offset,count);
        return os_mbuf_append(ctx->om,data,count+8)==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (kind == 5 && ctx->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        char json[2048];
        if (device_settings_json(json,sizeof(json))!=ESP_OK)return BLE_ATT_ERR_INSUFFICIENT_RES;
        /* Preserve the legacy characteristic within ATT's 512-byte value
         * limit. New clients retrieve the full settings via chunked RPC. */
        cJSON *settings=cJSON_Parse(json);if(!settings)return BLE_ATT_ERR_INSUFFICIENT_RES;
        const char *optional[]={"connectionsCoexist","startupPortal","controllerLimit","connectionMode","networkControllerLimit","bluetoothControllerLimit","bluetoothAlwaysAvailable","portalClientLimit","apNameDefault","bleNameDefault","apOpen","activeApOpen","defaultNamePending","webClients","playbackClients","showHeader","showFooter","headerHeight","footerHeight","barFields","headerFontSize","footerFontSize"};
        for(size_t i=0;i<sizeof(optional)/sizeof(optional[0]);++i)cJSON_DeleteItemFromObject(settings,optional[i]);
        cJSON_AddBoolToObject(settings,"settingsChunked",true);
        bool fits=cJSON_PrintPreallocated(settings,json,513,false);cJSON_Delete(settings);
        if(!fits)return BLE_ATT_ERR_INSUFFICIENT_RES;
        return os_mbuf_append(ctx->om,json,strlen(json))==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (kind == 2 && ctx->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        control_session_ble_peer(connection,true);
        prompter_status_t state;
        prompter_snapshot(&state, NULL);
        char address[16];
        wifi_settings_address(address, sizeof(address));
        char json[1024];
        int n = prompter_status_json(json,sizeof(json),&state);
        if(n<=0 || n>=(int)sizeof(json))return BLE_ATT_ERR_INSUFFICIENT_RES;
        unsigned mtu=ble_att_mtu(connection);
        unsigned write_bytes=mtu>6?mtu-6:17;
        if(write_bytes>509)write_bytes=509;
        int extra=snprintf(json+n-1,sizeof(json)-n+1,",\"ip\":\"%s\",\"bleWriteBytes\":%u}",address,write_bytes);
        n=n-1+extra;
        if (n < 0 || n >= sizeof(json)) return BLE_ATT_ERR_INSUFFICIENT_RES;
        return os_mbuf_append(ctx->om, json, n) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ctx->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;
    uint8_t packet[512];
    uint16_t n = OS_MBUF_PKTLEN(ctx->om);
    if (!n || n > sizeof(packet)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if (ble_hs_mbuf_to_flat(ctx->om, packet, sizeof(packet), NULL)) return BLE_ATT_ERR_UNLIKELY;
    esp_err_t err = ESP_ERR_INVALID_ARG;
    if(kind==6){
        if(packet[0]==0x20&&n==3){
            free(s_library_upload);s_library_upload=NULL;s_library_expected=le16(packet+1);s_library_received=0;
            if(s_library_expected&&s_library_expected<=57344){s_library_upload=malloc(s_library_expected+1);if(s_library_upload)err=ESP_OK;}
        }else if(packet[0]==0x21&&n>3&&s_library_upload&&le16(packet+1)==s_library_received&&n-3<=s_library_expected-s_library_received){
            memcpy(s_library_upload+s_library_received,packet+3,n-3);s_library_received+=n-3;err=ESP_OK;
        }else if(packet[0]==0x22&&n==1&&s_library_upload&&s_library_received==s_library_expected){
            s_library_upload[s_library_received]=0;
            if(!memchr(s_library_upload,0,s_library_received)){
                free(s_library_response);s_library_response=script_library_request(s_library_upload,TP_REQUEST_BLE);s_library_offset=0;
                cJSON *request=cJSON_Parse(s_library_upload);
                const cJSON *op=cJSON_GetObjectItem(request,"op"),*client=cJSON_GetObjectItem(request,"client");
                if(cJSON_IsString(op)&&!strcmp(op->valuestring,"presence")&&cJSON_IsString(client)){
                    char peer[32];snprintf(peer,sizeof(peer),"ble:%u",connection);
                    device_settings_presence_route(client->valuestring,peer,false,connection);
                }
                cJSON_Delete(request);
                if(s_library_response)err=ESP_OK;
            }
            free(s_library_upload);s_library_upload=NULL;
        }else if(packet[0]==0x23&&n==5){
            uint32_t offset=packet[1]|((uint32_t)packet[2]<<8)|((uint32_t)packet[3]<<16)|((uint32_t)packet[4]<<24);
            if(s_library_response&&offset<=strlen(s_library_response)){s_library_offset=offset;err=ESP_OK;}
        }
    } else if (kind == 3) {
        switch (packet[0]) {
        case 1:
            if (n == 2 && packet[1] <= 1) err = prompter_control(TP_PLAYING, packet[1], 0, 0, 0);
            break;
        case 2:
            if (n == 3) err = prompter_control(TP_SPEED, false, le16(packet + 1), 0, 0);
            break;
        case 3:
            if (n == 2) err = prompter_control(TP_FONT, false, 0, packet[1], 0);
            break;
        case 4:
            if (n == 3) err = prompter_control(TP_ROTATION, false, 0, 0, le16(packet + 1));
            break;
        case 5:
            if (n == 1) err = prompter_control(TP_REWIND, false, 0, 0, 0);
            break;
        case 6:
            if (n == 2) err = prompter_control_full(TP_BRIGHTNESS, false, 0, 0, 0, packet[1]);
            break;
        case 7:
            if (n == 2) err = prompter_control_extended(TP_LETTER_SPACING, false, 0, 0, 0, 0, packet[1], 0, 0);
            break;
        case 8:
            if (n == 2) err = prompter_control_extended(TP_LINE_SPACING, false, 0, 0, 0, 0, 0, packet[1], 0);
            break;
        case 9:
            if (n == 2) err = prompter_control_extended(TP_COUNTDOWN, false, 0, 0, 0, 0, 0, 0, packet[1]);
            break;
        case 10:
            if (n == 2 && ((packet[1] >= TP_DEVICE_RESTART && packet[1] <= TP_DEVICE_WAKE) || packet[1] == TP_DEVICE_SHUTDOWN))
                err = teleprompter_device_action((tp_device_action_t)packet[1]);
            break;
        case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: case 21: {
            prompter_status_t values={0};
            unsigned mask=0;
            if(packet[0]==11 && n==2 && packet[1]<=1){mask=TP_LOOP;values.loop=packet[1];}
            if(packet[0]==12 && n==2){mask=TP_TEXT_ALIGN;values.text_align=packet[1];}
            if(packet[0]==13 && n==3){mask=TP_MARGIN_LEFT;values.margin_left=le16(packet+1);}
            if(packet[0]==14 && n==3){mask=TP_MARGIN_RIGHT;values.margin_right=le16(packet+1);}
            if(packet[0]==15 && n==4){mask=TP_TEXT_COLOR;values.text_color=packet[1]|((uint32_t)packet[2]<<8)|((uint32_t)packet[3]<<16);}
            if(packet[0]==16 && n==2){mask=TP_MIRROR;values.mirror=packet[1];}
            if(packet[0]==17 && n==2 && packet[1]<=1){mask=TP_SIGHT_GUIDE;values.sight_guide=packet[1];}
            if(packet[0]==18 && n==2 && packet[1]<=1){mask=TP_CENTER_HIGHLIGHT;values.center_highlight=packet[1];}
            if(packet[0]==19 && n==2){mask=TP_SHUTTLE;values.shuttle=(int8_t)packet[1];}
            if(packet[0]==20 && n==2 && packet[1]<=1){mask=TP_AUTONEXT;values.auto_next=packet[1];}
            if(packet[0]==21 && n==2 && packet[1]<=1){mask=TP_BOLD;values.bold=packet[1];}
            if(packet[0]==22 && n==2 && packet[1]<=1){mask=TP_SHOW_INFO;values.show_info=packet[1];}
            if(mask)err=prompter_control_apply(mask,&values);
            break;
        }
        }
    } else if (kind == 5) {
        switch(packet[0]) {
        case 0:
            s_settings_active=false;
            if(n==3 && le16(packet+1)>0 && le16(packet+1)<sizeof(s_settings_upload)) {
                s_settings_expected=le16(packet+1);s_settings_received=0;s_settings_active=true;err=ESP_OK;
            }
            break;
        case 1:
            if(n>3 && s_settings_active && le16(packet+1)==s_settings_received &&
               n-3<=s_settings_expected-s_settings_received) {
                memcpy(s_settings_upload+s_settings_received,packet+3,n-3);
                s_settings_received+=n-3;err=ESP_OK;
            }
            break;
        case 2:
            if(n==1 && s_settings_active && s_settings_received==s_settings_expected) {
                s_settings_upload[s_settings_received]=0;
                if(!memchr(s_settings_upload,0,s_settings_received))err=device_settings_update(s_settings_upload);
            }
            s_settings_active=false;
            break;
        }
    } else if (kind == 4) {
        /* Kept discoverable for old clients, but SD library is now authoritative. */
        err=ESP_ERR_NOT_SUPPORTED;
    }
    if(err==ESP_OK && ((kind==3 && packet[0]!=10) || (kind==4 && packet[0]==0x12)))device_settings_activity();
    return err == ESP_OK ? 0 : BLE_ATT_ERR_VALUE_NOT_ALLOWED;
}

static const struct ble_gatt_svc_def s_services[] = {{
    .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &s_service.u,
    .characteristics = (struct ble_gatt_chr_def[]) {
        {.uuid=&s_status.u, .access_cb=access_value, .arg=(void *)2, .flags=BLE_GATT_CHR_F_READ},
        {.uuid=&s_control.u, .access_cb=access_value, .arg=(void *)3, .flags=BLE_GATT_CHR_F_WRITE},
        {.uuid=&s_script.u, .access_cb=access_value, .arg=(void *)4, .flags=BLE_GATT_CHR_F_WRITE},
        {.uuid=&s_settings.u, .access_cb=access_value, .arg=(void *)5, .flags=BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE},
        {.uuid=&s_library.u, .access_cb=access_value, .arg=(void *)6, .flags=BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP},
        {0},
    },
}, {0}};

static void advertise(void);
static void advertising_watch(struct ble_npl_event *event)
{
    (void)event;
    if(!ble_hs_synced()||atomic_load(&s_diagnostic_paused)||connection_count()>=CONFIG_BT_NIMBLE_MAX_CONNECTIONS)return;
    if(!ble_gap_adv_active()){
        ESP_LOGW(TAG,"Advertising inactive; restarting");
        advertise();
    }else if(s_fast_advertising&&esp_timer_get_time()>=s_fast_advertising_until){
        ble_gap_adv_stop();advertise();
    }
    ble_npl_callout_reset(&s_advertising_watch,ble_npl_time_ms_to_ticks32(2000));
}
static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Connection status=%d", event->connect.status);
        if(!event->connect.status){
            ble_session_t *session=NULL;
            for(unsigned i=0;i<CONFIG_BT_NIMBLE_MAX_CONNECTIONS;i++)if(!s_sessions[i].used){session=&s_sessions[i];break;}
            if(!session){ble_gap_terminate(event->connect.conn_handle,BLE_ERR_MEM_CAPACITY);break;}
            session->used=true;session->connection=event->connect.conn_handle;session->last_access=esp_timer_get_time();
            control_session_ble_peer(session->connection,false);
            device_settings_ble_count(connection_count());
            ble_npl_callout_reset(&s_connection_watch,ble_npl_time_ms_to_ticks32(5000));
            const struct ble_gap_upd_params params={.itvl_min=12,.itvl_max=24,.latency=0,.supervision_timeout=200};
            int rc=ble_gap_update_params(session->connection,&params);
            ESP_LOGI(TAG,"BLE peer=%u connected=%u params=%d",session->connection,connection_count(),rc);
        }
        advertise();
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        clear_session(find_session(event->disconnect.conn.conn_handle));
        if(!connection_count())ble_npl_callout_stop(&s_connection_watch);
        ESP_LOGI(TAG,"Disconnected reason=%d peers=%u",event->disconnect.reason,connection_count());
        s_fast_advertising_until=esp_timer_get_time()+INT64_C(30000000);
        advertise();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE: advertise(); break;
    default: break;
    }
    return 0;
}

static void advertise(void)
{
    /* GAP can deliver ADV_COMPLETE after a newer advertisement has already
     * started. Reconfiguring active advertising returns EBUSY and can race
     * reconnects; the watchdog still retries genuinely inactive advertising. */
    if (!ble_hs_synced()||atomic_load(&s_diagnostic_paused)||connection_count()>=CONFIG_BT_NIMBLE_MAX_CONNECTIONS||ble_gap_adv_active()) return;
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&s_service;
    fields.num_uuids128 = 1; fields.uuids128_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    struct ble_hs_adv_fields response = {0};
    response.name = (uint8_t *)ble_svc_gap_device_name();
    response.name_len = strlen((char *)response.name); response.name_is_complete = 1;
    if (!rc) rc = ble_gap_adv_rsp_set_fields(&response);
    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND; params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    /* Brief discovery window after boot/disconnect, then reduce radio use.
     * Connected links are unaffected. Keep diagnostic timing for A/B tests. */
    bool slow = atomic_load(&s_diagnostic_slow_advertising);
    s_fast_advertising=!slow&&esp_timer_get_time()<s_fast_advertising_until;
    params.itvl_min = BLE_GAP_ADV_ITVL_MS(slow ? 1600 : s_fast_advertising ? 100 : 400);
    params.itvl_max = BLE_GAP_ADV_ITVL_MS(slow ? 2000 : s_fast_advertising ? 150 : 600);
    if (!rc) rc = ble_gap_adv_start(s_address_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc) ESP_LOGE(TAG, "Advertising failed: %d", rc);
    else ESP_LOGI(TAG, "BLE_READY name=%s service=7d2a0001-4b7c-4d40-9e22-2f0b8d0c3a11 interval_ms=%d-%d",
                  ble_svc_gap_device_name(),slow?1600:s_fast_advertising?100:400,slow?2000:s_fast_advertising?150:600);
    ble_npl_callout_reset(&s_advertising_watch,ble_npl_time_ms_to_ticks32(2000));
}

static void refresh_name_event(struct ble_npl_event *event)
{
    (void)event;char name[30];
    portENTER_CRITICAL(&s_name_lock);memcpy(name,s_requested_name,sizeof(name));portEXIT_CRITICAL(&s_name_lock);
    if(!name[0]||firmware_update_in_progress())return;
    /* GAP names can change on a live connection without terminating it. */
    if(ble_svc_gap_device_name_set(name))return;
    device_settings_ble_name_applied(name);
    if(connection_count()<CONFIG_BT_NIMBLE_MAX_CONNECTIONS){ble_gap_adv_stop();advertise();}
}
esp_err_t bluetooth_control_refresh_name(const char *name)
{
    if(!s_started||!name||strlen(name)>29)return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&s_name_lock);snprintf(s_requested_name,sizeof(s_requested_name),"%s",name);portEXIT_CRITICAL(&s_name_lock);
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(),&s_name_event);return ESP_OK;
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (!rc) rc = ble_hs_id_infer_auto(0, &s_address_type);
    if (rc) { ESP_LOGE(TAG, "BLE address failed: %d", rc); return; }
    s_fast_advertising_until=esp_timer_get_time()+INT64_C(30000000);
    advertise();
}
static void diagnostic_event(struct ble_npl_event *event)
{
    (void)event;
    bool paused = atomic_load(&s_diagnostic_paused);
    if (paused) {
        ble_npl_callout_stop(&s_advertising_watch);
        ble_gap_adv_stop();
        for(unsigned i=0;i<CONFIG_BT_NIMBLE_MAX_CONNECTIONS;i++)if(s_sessions[i].used)
            ble_gap_terminate(s_sessions[i].connection,BLE_ERR_REM_USER_CONN_TERM);
    } else if (connection_count()<CONFIG_BT_NIMBLE_MAX_CONNECTIONS) {
        ble_gap_adv_stop();
        advertise();
    }
    ESP_LOGI(TAG, "USB diagnostic Bluetooth %s", paused ? "paused" : "resumed");
}
esp_err_t bluetooth_control_diagnostic_pause(bool paused)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;
    atomic_store(&s_diagnostic_paused, paused);
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_diagnostic_event);
    return ESP_OK;
}
esp_err_t bluetooth_control_diagnostic_advertising(bool slow)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;
    atomic_store(&s_diagnostic_slow_advertising, slow);
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_diagnostic_event);
    return ESP_OK;
}
static void on_reset(int reason)
{
    for(unsigned i=0;i<CONFIG_BT_NIMBLE_MAX_CONNECTIONS;i++)clear_session(&s_sessions[i]);
    ESP_LOGW(TAG,"Host reset: %d",reason);
    ble_npl_callout_stop(&s_connection_watch);ble_npl_callout_stop(&s_advertising_watch);
}
static void host_task(void *arg) { (void)arg; nimble_port_run(); nimble_port_freertos_deinit(); }

esp_err_t bluetooth_control_start(void)
{
    if (s_started) return ESP_ERR_INVALID_STATE;
    /* Older supplier firmware starts its controller by itself. Unsupported
     * controller RPCs must not stop the display or Wi-Fi startup. */
    esp_hosted_coprocessor_fwver_t version;
    bool known_version = esp_hosted_get_coprocessor_fwversion(&version) == ESP_OK;
    if (known_version)
        ESP_LOGI(TAG, "C6 firmware %lu.%lu.%lu", (unsigned long)version.major1,
                 (unsigned long)version.minor1, (unsigned long)version.patch1);
    bool legacy = known_version && (version.major1 < 2 ||
        (version.major1 == 2 && (version.minor1 < 5 ||
         (version.minor1 == 5 && version.patch1 < 2))));
    /* ESP-Hosted's bluetooth_design.md: before 2.5.2 the co-processor
     * enables its controller at boot. FeatureControl RPCs are unsupported
     * by the supplier's 1.4.1 firmware and each waits five seconds. */
    if (legacy) {
        ESP_LOGI(TAG, "Using legacy C6 controller enabled at boot");
    } else {
        esp_err_t init = esp_hosted_bt_controller_init();
        esp_err_t enable = esp_hosted_bt_controller_enable();
        ESP_LOGI(TAG, "Controller init=%s enable=%s", esp_err_to_name(init), esp_err_to_name(enable));
    }
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) return err;
    ble_npl_callout_init(&s_connection_watch,nimble_port_get_dflt_eventq(),connection_watch,NULL);
    ble_npl_callout_init(&s_advertising_watch,nimble_port_get_dflt_eventq(),advertising_watch,NULL);
    ble_npl_event_init(&s_diagnostic_event, diagnostic_event, NULL);
    ble_npl_event_init(&s_name_event,refresh_name_event,NULL);
    ble_hs_cfg.reset_cb = on_reset; ble_hs_cfg.sync_cb = on_sync;
    ble_svc_gap_init(); ble_svc_gatt_init();
    int rc = ble_gatts_count_cfg(s_services);
    if (!rc) rc = ble_gatts_add_svcs(s_services);
    char ble_name[30];
    device_settings_boot_names(NULL,0,ble_name,sizeof(ble_name));
    if (!rc) rc = ble_svc_gap_device_name_set(ble_name);
    if (rc) { nimble_port_deinit(); return ESP_FAIL; }
    s_started = true;
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}
