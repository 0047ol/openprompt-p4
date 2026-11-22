#include "device_settings.h"
#include "app_control.h"
#include "screen_bar_defaults.h"
#include "control_session.h"
#include "firmware_update.h"
#include "factory_reset.h"
#include "wifi_settings.h"
#include "bluetooth_control.h"
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include "cJSON.h"
#include "nvs.h"
#include "esp_timer.h"
#include "prompter_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    uint32_t version, auto_sleep_seconds;
    char ap_name[33], ble_name[30];
    uint8_t multi_control; /* Former zeroed reserved byte: default exclusive. */
    uint32_t language, show_info;
    char ap_password[64];
    uint32_t library_layout, library_sort;
    uint32_t auto_shutdown_seconds;
} settings_t;
static settings_t s_settings = {.version=6, .ap_name="Teleprompter-P4", .ble_name="Teleprompter-P4", .show_info=1, .library_sort=6};
static device_bars_t s_bars={.fields=TP_BAR_DEFAULT_FIELDS,
    .header_height=TP_BAR_DEFAULT_HEADER_HEIGHT,.footer_height=TP_BAR_DEFAULT_FOOTER_HEIGHT,
    .header=TP_BAR_DEFAULT_HEADER,.footer=TP_BAR_DEFAULT_FOOTER,
    .header_font_size=TP_BAR_DEFAULT_HEADER_FONT_SIZE,.footer_font_size=TP_BAR_DEFAULT_FOOTER_FONT_SIZE};
static atomic_int s_clock_offset;
static uint32_t bar_limit_fields(uint32_t fields)
{
    fields&=~TP_BAR_RETIRED_FIELDS;
    if(fields&TP_BAR_TIMECODE)fields&=~(TP_BAR_TIME|TP_BAR_DATE);
    if(fields&TP_BAR_NEXT)fields&=~TP_BAR_PREVIOUS;
    unsigned count=0;
    for(unsigned bit=0;bit<27;++bit){uint32_t flag=1u<<bit;
        if((fields&flag)&&!(flag&TP_BAR_HEADER_FIELDS)&&++count>TP_BAR_FOOTER_MAX)fields&=~flag;
    }
    return fields;
}
void device_settings_clock_hint(int offset_minutes){if(offset_minutes>=-840&&offset_minutes<=840)atomic_store(&s_clock_offset,offset_minutes);}
int device_settings_clock_offset(void){return atomic_load(&s_clock_offset);}
static char s_boot_ap[33], s_boot_ble[30];
static char s_boot_password[64];
static SemaphoreHandle_t s_mutex;
static nvs_handle_t s_nvs;
static uint32_t s_activity;
static int64_t s_active_us;
static bool s_asleep;
static uint32_t s_revision=1;
uint32_t device_settings_revision(void)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    uint32_t revision=s_revision;
    xSemaphoreGive(s_mutex);
    return revision;
}
static unsigned s_ble_connected;
typedef struct presence_client {
    struct presence_client *next;
    char id[33],peer[64];
    int64_t seen, status_seen, entered;
    bool reading,bluetooth,hotspot;
    int ble_handle;
    uint32_t socket_token;
} presence_client_t;
static presence_client_t *s_readers;
static char s_playback_owner[33];
static bool s_presence_managed, s_presence_pending;
static int s_presence_count;
#define READER_LEASE_US INT64_C(10000000)
static presence_client_t *presence_find(const char *id,bool create)
{
    for(presence_client_t *p=s_readers;p;p=p->next)if(!strcmp(p->id,id))return p;
    if(!create)return NULL;
    presence_client_t *p=calloc(1,sizeof(*p));if(!p)return NULL;
    snprintf(p->id,sizeof(p->id),"%s",id);p->ble_handle=-1;p->next=s_readers;s_readers=p;return p;
}
static int readers_count(int64_t now)
{
    int count=0;
    for(presence_client_t *p=s_readers;p;p=p->next)if(p->reading&&now-p->seen<READER_LEASE_US)++count;
    return count;
}
/* Called under s_mutex. Keep the incumbent while its foreground lease lives;
 * otherwise the oldest remaining playback viewer takes over. */
static presence_client_t *playback_owner(int64_t now)
{
    presence_client_t *oldest=NULL;
    for(presence_client_t *p=s_readers;p;p=p->next)if(p->reading&&now-p->seen<READER_LEASE_US){
        if(!strcmp(p->id,s_playback_owner))return p;
        if(!oldest||p->entered<oldest->entered)oldest=p;
    }
    snprintf(s_playback_owner,sizeof(s_playback_owner),"%s",oldest?oldest->id:"");return oldest;
}
bool device_settings_playback_allowed(const char *client)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    presence_client_t *owner=playback_owner(esp_timer_get_time());
    bool allowed=s_settings.multi_control||!owner||(client&&!strcmp(owner->id,client));
    xSemaphoreGive(s_mutex);return allowed;
}
bool device_settings_playback_patch_allowed(const char *text,const char *client)
{
    cJSON *json=cJSON_Parse(text);bool playback=false;const cJSON *item;
    cJSON_ArrayForEach(item,json)if(item->string&&(!strcmp(item->string,"multiControl")||!strcmp(item->string,"showInfo")||!strcmp(item->string,"showHeader")||!strcmp(item->string,"showFooter")||!strcmp(item->string,"barFields")||strstr(item->string,"Height")||strstr(item->string,"FontSize")))playback=true;
    cJSON_Delete(json);return !playback||device_settings_playback_allowed(client);
}
esp_err_t device_settings_presence(const char *id,bool reading,bool bluetooth)
{
    size_t length=id?strlen(id):0;
    if(length<8||length>32)return ESP_ERR_INVALID_ARG;
    for(size_t i=0;i<length;i++)if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    int64_t now=esp_timer_get_time();presence_client_t *p=presence_find(id,true);
    if(!p){xSemaphoreGive(s_mutex);return ESP_ERR_NO_MEM;}
    bool entering=reading&&(!p->reading||now-p->seen>=READER_LEASE_US);
    if(entering)p->entered=now;
    p->seen=now;p->reading=reading;p->bluetooth=bluetooth;
    if(!s_presence_managed||entering)s_presence_pending=true;
    s_presence_managed=true;
    playback_owner(now);
    xSemaphoreGive(s_mutex);return ESP_OK;
}
bool device_settings_presence_action(bool *wake)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    int count=readers_count(esp_timer_get_time());
    bool pending=s_presence_managed&&(s_presence_pending||(!count&&s_presence_count>0));
    s_presence_pending=false;s_presence_count=count;*wake=count>0;
    presence_client_t **link=&s_readers;
    while(*link){presence_client_t *p=*link;if(esp_timer_get_time()-p->seen>INT64_C(60000000)&&esp_timer_get_time()-p->status_seen>INT64_C(60000000)){*link=p->next;free(p);}else link=&p->next;}
    xSemaphoreGive(s_mutex);return pending;
}
bool device_settings_presence_managed(void){xSemaphoreTake(s_mutex,portMAX_DELAY);bool value=s_presence_managed;xSemaphoreGive(s_mutex);return value;}
void device_settings_presence_route(const char *id,const char *peer,bool hotspot,int ble_handle)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);presence_client_t *p=presence_find(id,false);
    if(p){snprintf(p->peer,sizeof(p->peer),"%s",peer?peer:"");p->hotspot=hotspot;p->ble_handle=ble_handle;}
    xSemaphoreGive(s_mutex);
}
void device_settings_presence_socket(const char *id,uint32_t token,bool open)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    presence_client_t *p=presence_find(id,open);
    if(p){
        if(open){p->socket_token=token;p->status_seen=esp_timer_get_time();}
        else if(p->socket_token==token){
            p->socket_token=0;
            /* A live-status socket can be replaced after a send timeout or
             * LRU eviction while the reader is still visible. Explicit
             * leave/radio events end reading immediately; otherwise keep
             * the last confirmed heartbeat until its bounded lease expires. */
        }
    }
    xSemaphoreGive(s_mutex);
}
void device_settings_presence_disconnect(const char *peer,int transport)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    for(presence_client_t *p=s_readers;p;p=p->next){
        bool match=transport==3?p->bluetooth:!p->bluetooth&&(transport==0||(transport==1&&p->hotspot)||(transport==2&&!p->hotspot));
        if(match&&(!peer||!strcmp(peer,p->peer)))p->reading=false;
    }
    xSemaphoreGive(s_mutex);
}
bool device_settings_presence_allows_wake(void)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    bool allowed=!s_presence_managed||readers_count(esp_timer_get_time())>0;
    xSemaphoreGive(s_mutex);return allowed;
}

void device_settings_client_seen(const char *id)
{
    size_t length=strlen(id);
    if(length<8 || length>32)return;
    for(size_t i=0;i<length;++i)if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return;
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    presence_client_t *p=presence_find(id,true);if(p)p->status_seen=esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}
void device_settings_ble_connected(bool connected)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);s_ble_connected=connected;
    xSemaphoreGive(s_mutex);
}
void device_settings_ble_count(unsigned count){xSemaphoreTake(s_mutex,portMAX_DELAY);s_ble_connected=count;xSemaphoreGive(s_mutex);}
void device_settings_ui(int *language, bool *show_info)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    if(language)*language=(int)s_settings.language;
    if(show_info)*show_info=s_settings.show_info!=0;
    xSemaphoreGive(s_mutex);
}

void device_settings_bars(device_bars_t *bars)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);*bars=s_bars;xSemaphoreGive(s_mutex);
}

/* Strict UTF-8 without control characters; limits count transmitted bytes. */
static bool valid_name(const char *name, size_t max)
{
    size_t n = strlen(name);
    if (!n || n > max || name[0]==' ' || name[n-1]==' ') return false;
    const unsigned char *p=(const unsigned char *)name;
    for (size_t i=0;i<n;) {
        unsigned c=p[i++], value, more, minimum;
        if (c<32 || c==127) return false;
        if (c<128) continue;
        if (c>=0xc2 && c<=0xdf) {value=c&31; more=1; minimum=0x80;}
        else if (c>=0xe0 && c<=0xef) {value=c&15; more=2; minimum=0x800;}
        else if (c>=0xf0 && c<=0xf4) {value=c&7; more=3; minimum=0x10000;}
        else return false;
        if (n-i<more) return false;
        while(more--) {c=p[i++]; if((c&0xc0)!=0x80)return false; value=(value<<6)|(c&63);}
        if(value<minimum || value>0x10ffff || (value>=0xd800 && value<=0xdfff))return false;
    }
    return true;
}
static bool valid_password(const char *password)
{
    size_t n=strlen(password);
    if(!n)return true;
    if(n<8 || n>63)return false;
    for(size_t i=0;i<n;++i)if((unsigned char)password[i]<32 || (unsigned char)password[i]>126)return false;
    return true;
}
static char s_default_names[2][30]={"提词器P4","Teleprompter-P4"};
static bool default_name(const char *name)
{
    return !strcmp(name,"Teleprompter-P4")||!strcmp(name,"提词器")||!strcmp(name,"提词器P4")||
        !strcmp(name,s_default_names[0])||!strcmp(name,s_default_names[1]);
}
static const char *display_name(const char *name,uint32_t language)
{
    return default_name(name)?s_default_names[language?1:0]:name;
}
void device_settings_radio_identity(const uint8_t mac[6])
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    snprintf(s_default_names[0],sizeof(s_default_names[0]),"提词器P4-%02X%02X",mac[4],mac[5]);
    snprintf(s_default_names[1],sizeof(s_default_names[1]),"Teleprompter-P4-%02X%02X",mac[4],mac[5]);
    snprintf(s_boot_ap,sizeof(s_boot_ap),"%s",display_name(s_settings.ap_name,s_settings.language));
    snprintf(s_boot_ble,sizeof(s_boot_ble),"%s",display_name(s_settings.ble_name,s_settings.language));
    ++s_revision;
    xSemaphoreGive(s_mutex);
}
void device_settings_display_name(char *name,size_t size)
{
    if(!name||!size)return;
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    const char *title=default_name(s_settings.ap_name)?
        (s_settings.language?"Teleprompter-P4":"提词器P4"):s_settings.ap_name;
    snprintf(name,size,"%s",title);
    xSemaphoreGive(s_mutex);
}
void device_settings_boot_password(char *password, size_t size)
{
    if(password && size)snprintf(password,size,"%s",s_boot_password);
}

esp_err_t device_settings_init(void)
{
    s_mutex=xSemaphoreCreateMutex();
    if(!s_mutex)return ESP_ERR_NO_MEM;
    esp_err_t err=nvs_open("tp_device",NVS_READWRITE,&s_nvs);
    if(err!=ESP_OK)return err;
    settings_t loaded={.version=6,.ap_name="Teleprompter-P4",.ble_name="Teleprompter-P4",.show_info=1,.library_sort=6}; size_t size=sizeof(loaded);
    err=nvs_get_blob(s_nvs,"settings",&loaded,&size);
    if(err==ESP_OK && ((size==sizeof(loaded) && loaded.version==6) ||
       (size==offsetof(settings_t,auto_shutdown_seconds) && (loaded.version==5||loaded.version==4)) ||
       (size==offsetof(settings_t,library_layout) && loaded.version==3) ||
       (size==offsetof(settings_t,ap_password) && loaded.version==2) ||
       (size==offsetof(settings_t,language) && loaded.version==1)) &&
       loaded.ap_name[32]==0 && loaded.ble_name[29]==0 &&
       valid_name(loaded.ap_name,32) && valid_name(loaded.ble_name,29) && loaded.auto_sleep_seconds<=86400 &&
       loaded.language<=1 && loaded.show_info<=1 && loaded.ap_password[63]==0 && valid_password(loaded.ap_password) &&
       loaded.library_layout<=1 && loaded.library_sort<=6 && loaded.auto_shutdown_seconds<=86400 && loaded.multi_control<=1){
        /* Migrate only the old factory password; custom passwords survive. */
        loaded.version=6;s_settings=loaded;
    }
    else if(err!=ESP_OK && err!=ESP_ERR_NVS_NOT_FOUND && err!=ESP_ERR_NVS_INVALID_LENGTH)return err;
    device_bars_t bars=s_bars;size=sizeof(bars);
    if(nvs_get_blob(s_nvs,"screen_bars",&bars,&size)==ESP_OK &&
       (size==sizeof(bars)||size==offsetof(device_bars_t,header_font_size)) &&
       bars.fields<=TP_BAR_ALL && bars.header_height>=24 && bars.header_height<=160 &&
       bars.footer_height>=24 && bars.footer_height<=160 &&
       bars.header_font_size>=12 && bars.header_font_size<=40 &&
       bars.footer_font_size>=12 && bars.footer_font_size<=40){
        if(size!=sizeof(bars))bars.fields|=TP_BAR_NAME;
        bars.fields=bar_limit_fields(bars.fields);s_bars=bars;
    }
    else s_bars.header=s_bars.footer=s_settings.show_info!=0;
    snprintf(s_boot_ap,sizeof(s_boot_ap),"%s",display_name(s_settings.ap_name,s_settings.language));
    snprintf(s_boot_ble,sizeof(s_boot_ble),"%s",display_name(s_settings.ble_name,s_settings.language));
    snprintf(s_boot_password,sizeof(s_boot_password),"%s",s_settings.ap_password);
    s_active_us=esp_timer_get_time();
    return ESP_OK;
}

void device_settings_boot_names(char *ap,size_t ap_size,char *ble,size_t ble_size)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    if(ap && ap_size)snprintf(ap,ap_size,"%s",s_boot_ap);
    if(ble && ble_size)snprintf(ble,ble_size,"%s",s_boot_ble);
    xSemaphoreGive(s_mutex);
}
void device_settings_ble_name_applied(const char *name)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    if(strcmp(s_boot_ble,name)){snprintf(s_boot_ble,sizeof(s_boot_ble),"%s",name);++s_revision;}
    xSemaphoreGive(s_mutex);
}
void device_settings_refresh_default_names(void)
{
    if(firmware_update_in_progress()||factory_reset_pending())return;
    char ap[33]={0},ble[30]={0};
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    if(default_name(s_settings.ap_name)&&default_name(s_boot_ap)&&strcmp(s_boot_ap,display_name(s_settings.ap_name,s_settings.language)))
        snprintf(ap,sizeof(ap),"%s",display_name(s_settings.ap_name,s_settings.language));
    if(default_name(s_settings.ble_name)&&default_name(s_boot_ble)&&strcmp(s_boot_ble,display_name(s_settings.ble_name,s_settings.language)))
        snprintf(ble,sizeof(ble),"%s",display_name(s_settings.ble_name,s_settings.language));
    xSemaphoreGive(s_mutex);
    /* Changing an in-use SSID disconnects clients. Apply only when idle. */
    if(ap[0]&&wifi_settings_refresh_ap_name(ap)==ESP_OK){
        xSemaphoreTake(s_mutex,portMAX_DELAY);
        snprintf(s_boot_ap,sizeof(s_boot_ap),"%s",ap);++s_revision;
        xSemaphoreGive(s_mutex);
    }
    if(ble[0])bluetooth_control_refresh_name(ble);
}
void device_settings_library_view(int *layout,int *sort,uint32_t *revision)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    *layout=s_settings.library_layout;*sort=s_settings.library_sort;*revision=s_revision;
    xSemaphoreGive(s_mutex);
}

esp_err_t device_settings_json(char *buffer,size_t size)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    settings_t settings=s_settings;device_bars_t bars=s_bars; bool asleep=s_asleep;
    char active_ap[33],active_ble[30];
    snprintf(active_ap,sizeof(active_ap),"%s",s_boot_ap);snprintf(active_ble,sizeof(active_ble),"%s",s_boot_ble);
    uint32_t revision=s_revision;int web_clients=0;unsigned ble_connected=s_ble_connected;
    int64_t now=esp_timer_get_time();
    int playback_clients=readers_count(now);
    for(presence_client_t *p=s_readers;p;p=p->next)if(p->status_seen&&now-p->status_seen<15000000)++web_clients;
    xSemaphoreGive(s_mutex);
    cJSON *json=cJSON_CreateObject();
    if(!json)return ESP_ERR_NO_MEM;
    bool ok=cJSON_AddNumberToObject(json,"boot",prompter_boot_id()) &&
        cJSON_AddNumberToObject(json,"revision",revision) &&
        cJSON_AddBoolToObject(json,"wifiConfigSupported",true) &&
        cJSON_AddBoolToObject(json,"startupPortal",false) &&
        cJSON_AddNumberToObject(json,"portalClientLimit",0) &&
        cJSON_AddStringToObject(json,"connectionMode",control_session_mode_name()) &&
        cJSON_AddNumberToObject(json,"controllerLimit",0) &&
        cJSON_AddNumberToObject(json,"networkControllerLimit",0) &&
        cJSON_AddNumberToObject(json,"bluetoothControllerLimit",0) &&
        cJSON_AddBoolToObject(json,"connectionsCoexist",true) &&
        cJSON_AddBoolToObject(json,"bluetoothAlwaysAvailable",true) &&
        cJSON_AddNumberToObject(json,"webClients",web_clients) &&
        cJSON_AddNumberToObject(json,"bleClients",ble_connected) &&
        cJSON_AddNumberToObject(json,"playbackClients",playback_clients) &&
        cJSON_AddNumberToObject(json,"libraryLayout",settings.library_layout) &&
        cJSON_AddNumberToObject(json,"librarySort",settings.library_sort) &&
        cJSON_AddNumberToObject(json,"autoSleepSeconds",settings.auto_sleep_seconds) &&
        cJSON_AddNumberToObject(json,"autoShutdownSeconds",settings.auto_shutdown_seconds) &&
        cJSON_AddStringToObject(json,"language",settings.language?"en":"zh-CN") &&
        cJSON_AddBoolToObject(json,"showInfo",settings.show_info!=0) &&
        cJSON_AddBoolToObject(json,"multiControl",settings.multi_control!=0) &&
        cJSON_AddBoolToObject(json,"showHeader",bars.header) &&
        cJSON_AddBoolToObject(json,"showFooter",bars.footer) &&
        cJSON_AddNumberToObject(json,"barFields",bars.fields) &&
        cJSON_AddBoolToObject(json,"timecodeSupported",true) &&
        cJSON_AddNumberToObject(json,"barFooterMax",TP_BAR_FOOTER_MAX) &&
        cJSON_AddNumberToObject(json,"headerHeight",bars.header_height) &&
        cJSON_AddNumberToObject(json,"footerHeight",bars.footer_height) &&
        cJSON_AddNumberToObject(json,"headerFontSize",bars.header_font_size) &&
        cJSON_AddNumberToObject(json,"footerFontSize",bars.footer_font_size) &&
        cJSON_AddStringToObject(json,"apName",display_name(settings.ap_name,settings.language)) &&
        cJSON_AddStringToObject(json,"bleName",display_name(settings.ble_name,settings.language)) &&
        cJSON_AddBoolToObject(json,"apNameDefault",default_name(settings.ap_name)) &&
        cJSON_AddBoolToObject(json,"bleNameDefault",default_name(settings.ble_name)) &&
        cJSON_AddStringToObject(json,"activeApName",active_ap) &&
        cJSON_AddStringToObject(json,"activeBleName",active_ble) &&
        cJSON_AddBoolToObject(json,"apOpen",!settings.ap_password[0]) &&
        cJSON_AddBoolToObject(json,"activeApOpen",!s_boot_password[0]) &&
        cJSON_AddBoolToObject(json,"defaultNamePending",(default_name(settings.ap_name)&&strcmp(display_name(settings.ap_name,settings.language),active_ap))||(default_name(settings.ble_name)&&strcmp(display_name(settings.ble_name,settings.language),active_ble))) &&
        cJSON_AddBoolToObject(json,"apPasswordPending",strcmp(settings.ap_password,s_boot_password)!=0) &&
        cJSON_AddBoolToObject(json,"restartRequired",((!default_name(settings.ap_name)||!default_name(active_ap))&&strcmp(display_name(settings.ap_name,settings.language),active_ap))||((!default_name(settings.ble_name)||!default_name(active_ble))&&strcmp(display_name(settings.ble_name,settings.language),active_ble))||strcmp(settings.ap_password,s_boot_password)) &&
        cJSON_AddBoolToObject(json,"asleep",asleep) && cJSON_PrintPreallocated(json,buffer,(int)size,false);
    cJSON_Delete(json);
    return ok?ESP_OK:ESP_ERR_NO_MEM;
}

esp_err_t device_settings_update(const char *text)
{
    if(teleprompter_power_blocked())return ESP_ERR_INVALID_STATE;
    if(firmware_update_in_progress()||factory_reset_pending())return ESP_ERR_INVALID_STATE;
    const char *end=NULL;
    cJSON *json=cJSON_ParseWithOpts(text,&end,true);
    if(!cJSON_IsObject(json)){cJSON_Delete(json);return ESP_ERR_INVALID_ARG;}
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    settings_t next=s_settings;
    device_bars_t next_bars=s_bars;
    bool valid=true, changed=false;
    cJSON *item;
    cJSON_ArrayForEach(item,json) {
        if(!strcmp(item->string,"multiControl")){
            if(!cJSON_IsBool(item)){valid=false;break;}next.multi_control=cJSON_IsTrue(item);
        }else if(!strcmp(item->string,"showHeader")||!strcmp(item->string,"showFooter")){
            if(!cJSON_IsBool(item)){valid=false;break;}
            if(!strcmp(item->string,"showHeader"))next_bars.header=cJSON_IsTrue(item);
            else next_bars.footer=cJSON_IsTrue(item);
            next.show_info=next_bars.header||next_bars.footer;
        }else if(!strcmp(item->string,"barFields")||!strcmp(item->string,"headerHeight")||!strcmp(item->string,"footerHeight")||
                 !strcmp(item->string,"headerFontSize")||!strcmp(item->string,"footerFontSize")){
            bool fields=!strcmp(item->string,"barFields");
            bool font=!strcmp(item->string,"headerFontSize")||!strcmp(item->string,"footerFontSize");
            if(!cJSON_IsNumber(item)||item->valuedouble<(fields?0:font?12:24)||item->valuedouble>(fields?TP_BAR_ALL:font?40:160)||item->valuedouble!=(double)item->valueint){valid=false;break;}
            if(fields)next_bars.fields=item->valueint&~TP_BAR_RETIRED_FIELDS;
            else if(!strcmp(item->string,"headerFontSize"))next_bars.header_font_size=item->valueint;
            else if(!strcmp(item->string,"footerFontSize"))next_bars.footer_font_size=item->valueint;
            else if(!strcmp(item->string,"headerHeight"))next_bars.header_height=item->valueint;
            else next_bars.footer_height=item->valueint;
        }else if(!strcmp(item->string,"libraryLayout")||!strcmp(item->string,"librarySort")) {
            bool layout=!strcmp(item->string,"libraryLayout");
            if(!cJSON_IsNumber(item)||item->valuedouble<0||item->valuedouble>(layout?1:6)||item->valuedouble!=(double)item->valueint){valid=false;break;}
            if(layout)next.library_layout=item->valueint;else next.library_sort=item->valueint;
        } else if(!strcmp(item->string,"autoSleepSeconds")||!strcmp(item->string,"autoShutdownSeconds")) {
            if(!cJSON_IsNumber(item) || item->valuedouble<0 || item->valuedouble>86400 ||
               item->valuedouble!=(double)item->valueint){valid=false;break;}
            if(!strcmp(item->string,"autoSleepSeconds"))next.auto_sleep_seconds=item->valueint;
            else next.auto_shutdown_seconds=item->valueint;
        } else if(!strcmp(item->string,"language")) {
            if(!cJSON_IsString(item) || (strcmp(item->valuestring,"zh-CN") && strcmp(item->valuestring,"en"))){valid=false;break;}
            next.language=!strcmp(item->valuestring,"en");
        } else if(!strcmp(item->string,"showInfo")) {
            if(!cJSON_IsBool(item)){valid=false;break;}
            next.show_info=cJSON_IsTrue(item);
            next_bars.header=next_bars.footer=next.show_info!=0;
        } else if(!strcmp(item->string,"apPassword")) {
            if(!cJSON_IsString(item) || !valid_password(item->valuestring)){valid=false;break;}
            memset(next.ap_password,0,sizeof(next.ap_password));
            strcpy(next.ap_password,item->valuestring);
        } else if(!strcmp(item->string,"apName") || !strcmp(item->string,"bleName")) {
            bool ap=!strcmp(item->string,"apName");
            if(!cJSON_IsString(item) || !valid_name(item->valuestring,ap?32:29)){valid=false;break;}
            char *target=ap?next.ap_name:next.ble_name;
            memset(target,0,ap?sizeof(next.ap_name):sizeof(next.ble_name));
            strcpy(target,item->valuestring);
        } else {valid=false;break;}
        changed=true;
    }
    esp_err_t err=ESP_ERR_INVALID_ARG;
    if(next_bars.fields!=bar_limit_fields(next_bars.fields))valid=false;
    if(valid && changed) {
        err=ESP_OK;
        if(memcmp(&next,&s_settings,sizeof(next))||memcmp(&next_bars,&s_bars,sizeof(next_bars))) {
            err=nvs_set_blob(s_nvs,"settings",&next,sizeof(next));
            if(err==ESP_OK)err=nvs_set_blob(s_nvs,"screen_bars",&next_bars,sizeof(next_bars));
            if(err==ESP_OK)err=nvs_commit(s_nvs);
        }
        if(err==ESP_OK){s_settings=next;s_bars=next_bars; ++s_revision; ++s_activity; s_active_us=esp_timer_get_time();}
    }
    xSemaphoreGive(s_mutex);
    cJSON_Delete(json);
    return err;
}

void device_settings_activity(void)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    ++s_activity; s_active_us=esp_timer_get_time();
    xSemaphoreGive(s_mutex);
}
uint32_t device_settings_activity_info(bool *expired)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    uint32_t result=s_activity;
    *expired=s_settings.auto_sleep_seconds && esp_timer_get_time()-s_active_us >= (int64_t)s_settings.auto_sleep_seconds*1000000;
    xSemaphoreGive(s_mutex);
    return result;
}
void device_settings_set_asleep(bool asleep)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);if(s_asleep!=asleep){s_asleep=asleep;++s_revision;}xSemaphoreGive(s_mutex);
}
bool device_settings_shutdown_due(void)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    bool due=s_settings.auto_shutdown_seconds&&esp_timer_get_time()-s_active_us>=(int64_t)s_settings.auto_shutdown_seconds*1000000;
    xSemaphoreGive(s_mutex);return due;
}
bool device_settings_asleep(void)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);bool asleep=s_asleep;xSemaphoreGive(s_mutex);return asleep;
}
