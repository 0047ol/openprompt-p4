#include "bluetooth_remote.h"
#include "remote_hid.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "nimble/nimble_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "prompter_state.h"
#include "device_settings.h"
#include "script_library.h"
#include "firmware_update.h"
#include "factory_reset.h"
#include "app_control.h"
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_FOUND 8
#define MAX_CHARS 16
#define NO_CONN BLE_HS_CONN_HANDLE_NONE
typedef struct { ble_addr_t address; char name[32]; int rssi; } found_t;
typedef struct { uint8_t version,enabled,reconnect,single,dbl,paired; ble_addr_t peer; char name[32]; } config_t;
typedef struct {
    config_t config; char state[20],error[40]; int battery;
    unsigned session,clicks; bool connected,ready,scanning,paused;
    found_t found[MAX_FOUND];unsigned count;
} view_t;
typedef struct { unsigned kind,token; config_t config; found_t peer; } command_t;
typedef struct { uint16_t definition,value,uuid,ccc,reference; uint8_t properties,id; bool subscribed; } characteristic_t;
static SemaphoreHandle_t lock;
static QueueHandle_t commands,actions;
static struct ble_npl_event command_event;
static struct ble_npl_callout timer;
static view_t view={.config={.version=1,.reconnect=1,.single=1,.dbl=2},.battery=-1};
static uint16_t connection=NO_CONN;
static atomic_uint remote_links;
static uint8_t own_type;
static bool synced,connecting,discovering,paused,manual_disconnect,worker_ready,scan_disconnect_before;
static unsigned scan_token;
static ble_addr_t target;
static char target_name[32];
static uint16_t hid_start,hid_end,battery_start,battery_end,battery_handle,battery_ccc;
static characteristic_t chars[MAX_CHARS];static unsigned char_count,char_index,subscriptions;
static uint8_t map_bytes[1024];static size_t map_size;static tp_hid_map report_map;
static tp_remote_clicks clicks;
static int64_t next_retry,last_battery;
static const char *TAG="tp_remote";
void ble_store_config_init(void);
static int gap_event(struct ble_gap_event *event,void *arg);
static void scan(bool manual);
static void descriptors_next(void);
static void discover(void);
#define LOCK() xSemaphoreTake(lock,portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(lock)
static view_t snapshot(void){view_t v;LOCK();v=view;UNLOCK();return v;}
static void state(const char *value,const char *error)
{LOCK();snprintf(view.state,sizeof(view.state),"%s",value);snprintf(view.error,sizeof(view.error),"%s",error?error:"");UNLOCK();}
static void save_config(void)
{
    config_t c=snapshot().config;nvs_handle_t h;
    if(nvs_open("tp_remote",NVS_READWRITE,&h)!=ESP_OK){state("error","settings_save_failed");return;}
    esp_err_t e=nvs_set_blob(h,"config",&c,sizeof(c));if(e==ESP_OK)e=nvs_commit(h);nvs_close(h);
    if(e!=ESP_OK)state("error","settings_save_failed");
}
static void fail(const char *error)
{
    state("error",error);discovering=false;next_retry=esp_timer_get_time()+15000000;
    if(connection!=NO_CONN)ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);
}
static bool capacity(void)
{return bluetooth_control_browser_connections()+bluetooth_remote_connections()<CONFIG_BT_NIMBLE_MAX_CONNECTIONS;}
unsigned bluetooth_remote_connections(void){return atomic_load(&remote_links);}
static void queue_click(unsigned kind)
{
    if(!kind||!worker_ready)return;
    view_t v=snapshot();unsigned action=kind==2?v.config.dbl:v.config.single;
    if(v.ready&&!v.paused&&v.config.enabled&&action){LOCK();view.clicks++;UNLOCK();xQueueSend(actions,&action,0);}
}
static void notify_input(uint16_t handle,const uint8_t *data,size_t length)
{
    if(handle==battery_handle){if(length&&data[0]<=100){LOCK();view.battery=data[0];UNLOCK();}return;}
    view_t v=snapshot();if(!v.ready||!v.config.enabled||paused)return;
    for(unsigned i=0;i<char_count;i++)if(chars[i].subscribed&&chars[i].value==handle){
        uint64_t now=esp_timer_get_time();queue_click(tp_remote_tick(&clicks,now));
        queue_click(tp_remote_input(&clicks,tp_hid_report(&report_map,chars[i].id,data,length),now,v.config.dbl!=0));break;
    }
}
static int battery_read(uint16_t conn,const struct ble_gatt_error *e,struct ble_gatt_attr *attr,void *arg)
{
    (void)arg;if(conn!=connection)return 0;
    if(!e->status&&attr){uint8_t b;if(ble_hs_mbuf_to_flat(attr->om,&b,1,NULL)==0&&b<=100){battery_handle=attr->handle;LOCK();view.battery=b;UNLOCK();}}
    return 0;
}
static int battery_subscribed(uint16_t conn,const struct ble_gatt_error *e,struct ble_gatt_attr *a,void *arg)
{(void)conn;(void)e;(void)a;(void)arg;return 0;}
static int battery_descriptors(uint16_t conn,const struct ble_gatt_error *e,uint16_t h,const struct ble_gatt_dsc *d,void *arg)
{
    (void)h;(void)arg;if(conn!=connection)return 0;
    if(!e->status&&d&&ble_uuid_u16(&d->uuid.u)==0x2902)battery_ccc=d->handle;
    if(e->status==BLE_HS_EDONE&&battery_ccc){uint8_t yes[]={1,0};ble_gattc_write_flat(conn,battery_ccc,yes,2,battery_subscribed,NULL);}
    return 0;
}
static int battery_chars(uint16_t conn,const struct ble_gatt_error *e,const struct ble_gatt_chr *c,void *arg)
{
    (void)arg;if(conn!=connection)return 0;
    if(!e->status&&c&&ble_uuid_u16(&c->uuid.u)==0x2a19)battery_handle=c->val_handle;
    if(e->status==BLE_HS_EDONE&&battery_handle){
        ble_gattc_read(conn,battery_handle,battery_read,NULL);
        if(battery_handle<battery_end)ble_gattc_disc_all_dscs(conn,battery_handle,battery_end,battery_descriptors,NULL);
    }
    return 0;
}
static void ready(void)
{
    if(!subscriptions){fail("unsupported_hid_report");return;}
    discovering=false;clicks=(tp_remote_clicks){0};
    LOCK();view.ready=true;view.session++;view.config.paired=1;view.config.enabled=1;
    struct ble_gap_conn_desc desc;if(!ble_gap_conn_find(connection,&desc))view.config.peer=desc.peer_id_addr;else view.config.peer=target;
    snprintf(view.config.name,sizeof(view.config.name),"%s",target_name);UNLOCK();state("connected",NULL);save_config();
    if(battery_start)ble_gattc_disc_all_chrs(connection,battery_start,battery_end,battery_chars,NULL);
    last_battery=esp_timer_get_time();ESP_LOGI(TAG,"HID ready subscriptions=%u",subscriptions);
}
static int subscribed(uint16_t conn,const struct ble_gatt_error *e,struct ble_gatt_attr *attr,void *arg)
{
    (void)attr;(void)arg;if(conn!=connection)return 0;
    if(e->status){fail("hid_subscribe_failed");return 0;}
    chars[char_index].subscribed=true;subscriptions++;char_index++;descriptors_next();return 0;
}
static int reference_read(uint16_t conn,const struct ble_gatt_error *e,struct ble_gatt_attr *attr,void *arg)
{
    (void)arg;if(conn!=connection)return 0;
    uint8_t value[2];characteristic_t *c=&chars[char_index];
    if(e->status||!attr||OS_MBUF_PKTLEN(attr->om)!=2||ble_hs_mbuf_to_flat(attr->om,value,2,NULL)){fail("hid_reference_failed");return 0;}
    c->id=value[0];bool supported=false;
    for(unsigned i=0;i<report_map.count;i++)if(report_map.fields[i].id==c->id)supported=true;
    if(value[1]==1&&supported&&c->ccc){uint8_t yes[]={1,0};if(ble_gattc_write_flat(conn,c->ccc,yes,2,subscribed,NULL))fail("hid_subscribe_failed");}
    else{char_index++;descriptors_next();}return 0;
}
static int report_descriptors(uint16_t conn,const struct ble_gatt_error *e,uint16_t handle,const struct ble_gatt_dsc *d,void *arg)
{
    (void)handle;(void)arg;if(conn!=connection)return 0;characteristic_t *c=&chars[char_index];
    if(!e->status&&d){uint16_t u=ble_uuid_u16(&d->uuid.u);if(u==0x2902)c->ccc=d->handle;if(u==0x2908)c->reference=d->handle;}
    else if(e->status==BLE_HS_EDONE){
        if(c->reference){if(ble_gattc_read(conn,c->reference,reference_read,NULL))fail("hid_reference_failed");}
        else{char_index++;descriptors_next();}
    }else fail("hid_discovery_failed");return 0;
}
static void descriptors_next(void)
{
    while(char_index<char_count){
        characteristic_t *c=&chars[char_index];
        if(c->uuid==0x2a4d&&(c->properties&BLE_GATT_CHR_PROP_NOTIFY)){
            uint16_t end=char_index+1<char_count?chars[char_index+1].definition-1:hid_end;
            if(c->value<end){if(ble_gattc_disc_all_dscs(connection,c->value,end,report_descriptors,NULL))fail("hid_discovery_failed");return;}
        }char_index++;
    }ready();
}
static int map_read(uint16_t conn,const struct ble_gatt_error *e,struct ble_gatt_attr *attr,void *arg)
{
    (void)arg;if(conn!=connection)return 0;
    if(!e->status&&attr){size_t n=OS_MBUF_PKTLEN(attr->om);
        if(attr->offset!=map_size||map_size+n>sizeof(map_bytes)||ble_hs_mbuf_to_flat(attr->om,map_bytes+map_size,n,NULL)){fail("unsupported_hid_report");return BLE_HS_EAPP;}
        map_size+=n;
    }else if(e->status==BLE_HS_EDONE){
        if(!tp_hid_parse(&report_map,map_bytes,map_size)){fail("unsupported_hid_report");return 0;}
        char_index=0;descriptors_next();
    }else{fail("hid_map_failed");}return 0;
}
static int hid_chars(uint16_t conn,const struct ble_gatt_error *e,const struct ble_gatt_chr *c,void *arg)
{
    (void)arg;if(conn!=connection)return 0;
    if(!e->status&&c){if(char_count>=MAX_CHARS){fail("unsupported_hid_report");return BLE_HS_EAPP;}
        chars[char_count++]=(characteristic_t){.definition=c->def_handle,.value=c->val_handle,.uuid=ble_uuid_u16(&c->uuid.u),.properties=c->properties};
    }else if(e->status==BLE_HS_EDONE){
        for(unsigned i=0;i<char_count;i++)if(chars[i].uuid==0x2a4b){map_size=0;if(ble_gattc_read_long(conn,chars[i].value,0,map_read,NULL))fail("hid_map_failed");return 0;}
        fail("unsupported_hid_report");
    }else fail("hid_discovery_failed");return 0;
}
static int service_found(uint16_t conn,const struct ble_gatt_error *e,const struct ble_gatt_svc *s,void *arg)
{
    (void)arg;if(conn!=connection)return 0;
    if(!e->status&&s){uint16_t u=ble_uuid_u16(&s->uuid.u);
        if(u==0x1812){hid_start=s->start_handle;hid_end=s->end_handle;}
        if(u==0x180f){battery_start=s->start_handle;battery_end=s->end_handle;}
    }else if(e->status==BLE_HS_EDONE){
        if(!hid_start){fail("not_hid_remote");return 0;}
        if(ble_gattc_disc_all_chrs(conn,hid_start,hid_end,hid_chars,NULL))fail("hid_discovery_failed");
    }else fail("hid_discovery_failed");return 0;
}
static void discover(void)
{
    if(discovering||connection==NO_CONN)return;
    discovering=true;
    hid_start=hid_end=battery_start=battery_end=battery_handle=battery_ccc=0;
    char_count=char_index=subscriptions=0;memset(chars,0,sizeof(chars));state("discovering",NULL);
    if(ble_gattc_disc_all_svcs(connection,service_found,NULL))fail("hid_discovery_failed");
}
static void connect_peer(const ble_addr_t *peer,const char *name)
{
    if(!synced||paused||connecting||connection!=NO_CONN||!capacity()){state("error","bluetooth_capacity");return;}
    scan_token=0;
    ble_gap_disc_cancel();LOCK();view.scanning=false;UNLOCK();
    target=*peer;snprintf(target_name,sizeof(target_name),"%s",name);manual_disconnect=false;
    const struct ble_gap_conn_params params={.scan_itvl=0x60,.scan_window=0x30,.itvl_min=24,.itvl_max=40,.latency=4,.supervision_timeout=400,.min_ce_len=0,.max_ce_len=0};
    int rc=ble_gap_connect(own_type,peer,12000,&params,gap_event,NULL);
    if(rc){state("error","remote_connect_failed");next_retry=esp_timer_get_time()+15000000;}
    else{connecting=true;atomic_store(&remote_links,1);state("connecting",NULL);bluetooth_control_remote_capacity_changed();}
}
static bool same_address(const ble_addr_t *a,const ble_addr_t *b){return a->type==b->type&&!memcmp(a->val,b->val,6);}
static int gap_event(struct ble_gap_event *e,void *arg)
{
    (void)arg;
    if(e->type==BLE_GAP_EVENT_DISC){
        view_t v=snapshot();struct ble_hs_adv_fields f;if(ble_hs_adv_parse_fields(&f,e->disc.data,e->disc.length_data))return 0;
        char name[32]={0};if(f.name&&f.name_len){unsigned n=f.name_len;if(n>31)n=31;memcpy(name,f.name,n);}
        if(v.config.enabled&&v.config.paired&&!manual_disconnect&&same_address(&v.config.peer,&e->disc.addr)&&!v.scanning){connect_peer(&e->disc.addr,v.config.name);return 0;}
        bool hid=false;for(unsigned i=0;i<f.num_uuids16;i++)if(f.uuids16[i].value==0x1812)hid=true;
        if(!hid&&!strstr(name,"ULANZI")&&!strstr(name,"ulanzi"))return 0;
        LOCK();unsigned i=0;while(i<view.count&&!same_address(&view.found[i].address,&e->disc.addr))i++;
        if(i<MAX_FOUND){if(i==view.count)view.count++;view.found[i].address=e->disc.addr;view.found[i].rssi=e->disc.rssi;if(name[0])snprintf(view.found[i].name,32,"%s",name);else if(!view.found[i].name[0])snprintf(view.found[i].name,32,"BLE HID");}UNLOCK();
    }else if(e->type==BLE_GAP_EVENT_DISC_COMPLETE){
        LOCK();bool manual=view.scanning;view.scanning=false;UNLOCK();if(manual){manual_disconnect=scan_disconnect_before;state("idle",NULL);}
    }else if(e->type==BLE_GAP_EVENT_CONNECT){
        connecting=false;
        if(e->connect.status){atomic_store(&remote_links,0);next_retry=esp_timer_get_time()+10000000;state("error","remote_connect_failed");bluetooth_control_remote_capacity_changed();return 0;}
        connection=e->connect.conn_handle;
        if(manual_disconnect||!snapshot().config.enabled){ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);return 0;}
        LOCK();view.connected=true;view.battery=-1;UNLOCK();state("pairing",NULL);
        struct ble_gap_conn_desc d;if(!ble_gap_conn_find(connection,&d)&&d.sec_state.encrypted)discover();
        else {int rc=ble_gap_security_initiate(connection);if(rc&&rc!=BLE_HS_EALREADY)fail("remote_pair_failed");}
    }else if(e->type==BLE_GAP_EVENT_ENC_CHANGE){
        if(e->enc_change.conn_handle==connection){if(!e->enc_change.status)discover();else fail("remote_pair_failed");}
    }else if(e->type==BLE_GAP_EVENT_DISCONNECT){
        bool had_error=snapshot().error[0]!=0;connection=NO_CONN;connecting=discovering=false;atomic_store(&remote_links,0);clicks=(tp_remote_clicks){0};xQueueReset(actions);
        LOCK();view.connected=view.ready=false;view.battery=-1;UNLOCK();
        if(!had_error)state("disconnected",NULL);
        next_retry=esp_timer_get_time()+5000000;bluetooth_control_remote_capacity_changed();
    }else if(e->type==BLE_GAP_EVENT_NOTIFY_RX){
        if(e->notify_rx.conn_handle==connection){uint8_t data[128];size_t n=OS_MBUF_PKTLEN(e->notify_rx.om);if(n<=sizeof(data)&&!ble_hs_mbuf_to_flat(e->notify_rx.om,data,n,NULL))notify_input(e->notify_rx.attr_handle,data,n);}
    }else if(e->type==BLE_GAP_EVENT_REPEAT_PAIRING){
        /* User must explicitly Forget before replacing security keys. */
        return BLE_GAP_REPEAT_PAIRING_IGNORE;
    }else if(e->type==BLE_GAP_EVENT_PASSKEY_ACTION){fail("pairing_code_unsupported");}
    return 0;
}
static void scan(bool manual)
{
    if(!synced||paused||connecting||connection!=NO_CONN)return;
    if(ble_gap_disc_active())ble_gap_disc_cancel();
    if(manual)manual_disconnect=true;
    struct ble_gap_disc_params p={.itvl=480,.window=48,.passive=0,.filter_duplicates=1,.limited=0};
    LOCK();view.scanning=manual;if(manual)view.count=0;UNLOCK();
    int rc=ble_gap_disc(own_type,manual?10000:3000,&p,gap_event,NULL);
    if(rc){if(manual)manual_disconnect=scan_disconnect_before;LOCK();view.scanning=false;UNLOCK();state("error","remote_scan_failed");}
    else if(manual)state("scanning",NULL);
}
static void command_received(struct ble_npl_event *event)
{
    (void)event;command_t c;
    while(xQueueReceive(commands,&c,0)==pdTRUE){
        if(c.kind==1){LOCK();view.config=c.config;UNLOCK();save_config();
            if(!c.config.enabled){scan_token=0;manual_disconnect=true;clicks=(tp_remote_clicks){0};xQueueReset(actions);ble_gap_disc_cancel();if(connecting)ble_gap_conn_cancel();if(connection!=NO_CONN)ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);LOCK();view.scanning=false;UNLOCK();state("disabled",NULL);}
            else {manual_disconnect=false;next_retry=0;}
        }else if(c.kind==2){if(!scan_token)scan_disconnect_before=manual_disconnect;manual_disconnect=true;scan_token=c.token;scan(true);}
        else if(c.kind==3){LOCK();view.config.enabled=1;UNLOCK();connect_peer(&c.peer.address,c.peer.name);}
        else if(c.kind==4||c.kind==5){
            scan_token=0;
            manual_disconnect=true;clicks=(tp_remote_clicks){0};xQueueReset(actions);ble_gap_disc_cancel();if(connecting)ble_gap_conn_cancel();
            if(connection!=NO_CONN)ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);
            if(c.kind==5){view_t v=snapshot();if(v.config.paired)ble_store_util_delete_peer(&v.config.peer);LOCK();view.config.paired=0;view.config.enabled=0;view.config.name[0]=0;memset(&view.config.peer,0,sizeof(view.config.peer));UNLOCK();save_config();}
            LOCK();view.scanning=false;UNLOCK();state("disconnected",NULL);
        }else if(c.kind==7){
            if(c.token&&c.token==scan_token){scan_token=0;if(snapshot().scanning)ble_gap_disc_cancel();if(!connecting&&connection==NO_CONN){manual_disconnect=scan_disconnect_before;LOCK();view.scanning=false;UNLOCK();state("idle",NULL);}}
        }else if(c.kind==6){paused=c.config.enabled;LOCK();view.paused=paused;UNLOCK();
            if(paused){ble_gap_disc_cancel();if(connecting)ble_gap_conn_cancel();if(connection!=NO_CONN)ble_gap_terminate(connection,BLE_ERR_REM_USER_CONN_TERM);}
        }
    }
}
static void timer_tick(struct ble_npl_event *event)
{
    (void)event;int64_t now=esp_timer_get_time();view_t v=snapshot();
    if(v.ready){queue_click(tp_remote_tick(&clicks,now));
        if(battery_handle&&now-last_battery>60000000&&!firmware_update_in_progress()){last_battery=now;ble_gattc_read(connection,battery_handle,battery_read,NULL);}
    }else if(synced&&!paused&&!manual_disconnect&&v.config.enabled&&v.config.paired&&v.config.reconnect&&connection==NO_CONN&&!connecting&&!ble_gap_disc_active()&&now>=next_retry&&!firmware_update_in_progress()&&capacity()){
        next_retry=now+15000000;scan(false);
    }
    ble_npl_callout_reset(&timer,ble_npl_time_ms_to_ticks32(v.ready?20:1000));
}
static void action_worker(void *unused)
{
    (void)unused;unsigned action;int64_t active_until=0;
    for(;;){
        bool received=xQueueReceive(actions,&action,pdMS_TO_TICKS(250))==pdTRUE;view_t v=snapshot();
        bool allowed=v.ready&&v.config.enabled&&!v.paused&&!firmware_update_in_progress()&&!factory_reset_pending()&&!teleprompter_power_blocked()&&!script_library_maintenance();
        prompter_status_t s;prompter_snapshot(&s,NULL);
        if(received&&allowed&&s.length){
            if(device_settings_asleep()){teleprompter_device_action(TP_DEVICE_WAKE);continue;}
            device_settings_activity();active_until=esp_timer_get_time()+30000000;
            if(action==1){s.playing=!(s.playing||s.countdown_remaining_ms);script_library_control(TP_PLAYING,&s);}
            else if(action==2||action==3){
                if(s.page_mode){s.page_step=action==2?1:-1;script_library_control(TP_PAGE_STEP,&s);}
                else script_library_remote_step(action==2?1:-1);
            }
        }
        prompter_snapshot(&s,NULL);
        device_settings_remote_reading(allowed&&active_until&&(s.playing||s.countdown_remaining_ms||esp_timer_get_time()<active_until));
        if(!allowed)active_until=0;
    }
}
void bluetooth_remote_pause(bool value)
{
    if(!commands)return;
    command_t c={.kind=6,.config.enabled=value};if(xQueueSend(commands,&c,0)==pdTRUE)ble_npl_eventq_put(nimble_port_get_dflt_eventq(),&command_event);
}
void bluetooth_remote_reset(void)
{
    if(!lock)return;
    synced=false;connection=NO_CONN;connecting=discovering=false;atomic_store(&remote_links,0);clicks=(tp_remote_clicks){0};xQueueReset(actions);
    LOCK();view.connected=view.ready=view.scanning=false;view.battery=-1;UNLOCK();
}
void bluetooth_remote_sync(uint8_t type)
{if(!lock)return;own_type=type;synced=true;manual_disconnect=false;ble_npl_callout_reset(&timer,ble_npl_time_ms_to_ticks32(1000));}
void bluetooth_remote_init(void)
{
    lock=xSemaphoreCreateMutex();commands=xQueueCreate(4,sizeof(command_t));actions=xQueueCreate(4,sizeof(unsigned));
    if(!lock||!commands||!actions){if(lock)vSemaphoreDelete(lock);if(commands)vQueueDelete(commands);if(actions)vQueueDelete(actions);lock=NULL;commands=actions=NULL;return;}
    nvs_handle_t h;size_t n=sizeof(config_t);config_t c;
    if(nvs_open("tp_remote",NVS_READONLY,&h)==ESP_OK){if(nvs_get_blob(h,"config",&c,&n)==ESP_OK&&n==sizeof(c)&&c.version==1&&c.single<=3&&c.dbl<=3){c.name[31]=0;view.config=c;}nvs_close(h);}
    snprintf(view.state,sizeof(view.state),"%s",view.config.enabled?"disconnected":"disabled");
    ble_npl_event_init(&command_event,command_received,NULL);ble_npl_callout_init(&timer,nimble_port_get_dflt_eventq(),timer_tick,NULL);
    ble_hs_cfg.sm_io_cap=BLE_HS_IO_NO_INPUT_OUTPUT;ble_hs_cfg.sm_bonding=1;
    ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.store_status_cb=ble_store_util_status_rr;ble_store_config_init();
    worker_ready=xTaskCreate(action_worker,"remote_keys",8192,NULL,2,NULL)==pdPASS;
    if(!worker_ready)state("error","out_of_memory");
}
static void address_text(const ble_addr_t *a,char text[24])
{snprintf(text,24,"%02X:%02X:%02X:%02X:%02X:%02X",a->val[5],a->val[4],a->val[3],a->val[2],a->val[1],a->val[0]);}
cJSON *bluetooth_remote_request(const cJSON *request)
{
    cJSON *out=cJSON_CreateObject();const char *err=NULL;
    if(!lock||!commands||!worker_ready){err="remote_unavailable";goto end;}
    const cJSON *a=cJSON_GetObjectItem(request,"action");const char *action=cJSON_IsString(a)?a->valuestring:"status";
    command_t cmd={0};view_t v=snapshot();
    if(!strcmp(action,"settings")){
        cmd.kind=1;cmd.config=v.config;const cJSON *item;
        cJSON_ArrayForEach(item,request){if(!strcmp(item->string,"enabled")||!strcmp(item->string,"autoReconnect")){
            if(!cJSON_IsBool(item)){err="invalid_request";goto end;}if(!strcmp(item->string,"enabled"))cmd.config.enabled=cJSON_IsTrue(item);else cmd.config.reconnect=cJSON_IsTrue(item);
        }else if(!strcmp(item->string,"singleClick")||!strcmp(item->string,"doubleClick")){
            if(!cJSON_IsNumber(item)||item->valuedouble!=item->valueint||item->valueint<0||item->valueint>3){err="invalid_request";goto end;}
            if(!strcmp(item->string,"singleClick"))cmd.config.single=item->valueint;else cmd.config.dbl=item->valueint;
        }}
    }else if(!strcmp(action,"scan")){if(v.connected||connecting){err="remote_already_connected";goto end;}cmd.kind=2;}
    else if(!strcmp(action,"scanCancel"))cmd.kind=7;
    else if(!strcmp(action,"connect")){
        const cJSON *index=cJSON_GetObjectItem(request,"index");
        if(!cJSON_IsNumber(index)||index->valueint<0||(unsigned)index->valueint>=v.count||index->valuedouble!=index->valueint){err="invalid_request";goto end;}
        if(v.connected||connecting){err="remote_already_connected";goto end;}cmd.kind=3;cmd.peer=v.found[index->valueint];
    }else if(!strcmp(action,"disconnect"))cmd.kind=4;
    else if(!strcmp(action,"forget")){if(!cJSON_IsTrue(cJSON_GetObjectItem(request,"confirmed"))){err="confirmation_required";goto end;}cmd.kind=5;}
    else if(strcmp(action,"status")){err="invalid_request";goto end;}
    if(cmd.kind){
        const cJSON *token=cJSON_GetObjectItem(request,"scanToken");cmd.token=cJSON_IsNumber(token)&&token->valuedouble>=1&&token->valuedouble<=4294967295.0?(unsigned)token->valuedouble:0;
        if(firmware_update_in_progress()||factory_reset_pending()||teleprompter_power_blocked()){err="device_busy";goto end;}
        if(xQueueSend(commands,&cmd,0)!=pdTRUE){err="device_busy";goto end;}ble_npl_eventq_put(nimble_port_get_dflt_eventq(),&command_event);}
    cJSON_AddStringToObject(out,"state",v.state);cJSON_AddStringToObject(out,"errorDetail",v.error);
    cJSON_AddBoolToObject(out,"enabled",v.config.enabled);cJSON_AddBoolToObject(out,"autoReconnect",v.config.reconnect);
    cJSON_AddBoolToObject(out,"paired",v.config.paired);cJSON_AddBoolToObject(out,"connected",v.connected);cJSON_AddBoolToObject(out,"ready",v.ready);
    cJSON_AddBoolToObject(out,"scanning",v.scanning);cJSON_AddStringToObject(out,"name",v.config.name);
    cJSON_AddNumberToObject(out,"battery",v.ready?v.battery:-1);cJSON_AddNumberToObject(out,"session",v.session);
    cJSON_AddNumberToObject(out,"clicks",v.clicks);cJSON_AddNumberToObject(out,"singleClick",v.config.single);cJSON_AddNumberToObject(out,"doubleClick",v.config.dbl);
    cJSON_AddNumberToObject(out,"connectionLimit",CONFIG_BT_NIMBLE_MAX_CONNECTIONS);
    cJSON *found=cJSON_AddArrayToObject(out,"devices");
    for(unsigned i=0;i<v.count;i++){cJSON *d=cJSON_CreateObject();char text[24];address_text(&v.found[i].address,text);cJSON_AddNumberToObject(d,"index",i);cJSON_AddStringToObject(d,"name",v.found[i].name);cJSON_AddStringToObject(d,"address",text);cJSON_AddNumberToObject(d,"rssi",v.found[i].rssi);cJSON_AddItemToArray(found,d);}
end:cJSON_AddBoolToObject(out,"ok",!err);if(err)cJSON_AddStringToObject(out,"error",err);return out;
}
