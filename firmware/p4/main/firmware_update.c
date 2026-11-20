#include "firmware_update.h"
#include "factory_reset.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <stdatomic.h>
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "mbedtls/base64.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "script_library.h"
#include "app_control.h"

#define UPDATE_DIR "/sdcard/firmware"
#define PREFIX "teleprompter-p4-"
#define HEADER_SIZE (sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t)+sizeof(esp_app_desc_t))
static SemaphoreHandle_t lock;
static nvs_handle_t nv;
static bool automatic, online_automatic=true, healthy, started, sd_pending, sd_apply, save_to_sd;
static const esp_partition_t *target;
static esp_ota_handle_t handle;
static size_t expected, received;
static uint32_t token;
static int64_t touched, reboot_at;
static esp_app_desc_t candidate;
static char phase[24]="idle", detail[96]="", available[128]="";
static const char *TAG="firmware_update";
static atomic_uint display_progress;

static void publish_progress(void)
{
    unsigned stage=reboot_at?TP_UPDATE_RESTARTING:started?TP_UPDATE_WRITING:sd_pending?TP_UPDATE_CHECKING:TP_UPDATE_IDLE;
    unsigned percent=expected?(unsigned)((uint64_t)received*100/expected):0;
    atomic_store(&display_progress,(stage<<8)|(percent>100?100:percent));
}
tp_update_phase_t firmware_update_progress(unsigned *percent)
{
    unsigned value=atomic_load(&display_progress);
    if(percent)*percent=value&255;
    return (tp_update_phase_t)(value>>8);
}

bool firmware_update_allows_shutdown(void)
{
    if(!lock)return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool allowed=healthy&&!started&&!sd_pending&&!reboot_at&&!factory_reset_pending();
    xSemaphoreGive(lock);return allowed;
}
bool firmware_update_allows_restart(void)
{
    if(!lock)return false;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool allowed=healthy&&!started&&!sd_pending&&!factory_reset_pending();
    xSemaphoreGive(lock);return allowed;
}
bool firmware_update_in_progress(void)
{
    return firmware_update_progress(NULL)!=TP_UPDATE_IDLE;
}

static cJSON *reply(const char *error)
{
    cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"ok",!error);
    if(error)cJSON_AddStringToObject(j,"error",error);
    return j;
}
static uint64_t version_number(const char *s)
{
    unsigned date=0,build=0;int end=0;
    if(sscanf(s,"%u.%u%n",&date,&build,&end)!=2||s[end]||date<20260101||date>20991231||build>999999)return 0;
    return (uint64_t)date*1000000+build;
}
static bool rejected_hash(const uint8_t *hash)
{
    uint8_t bad[32];size_t n=sizeof(bad);
    char key[15];snprintf(key,sizeof(key),"bad%02x%02x%02x%02x%02x",hash[0],hash[1],hash[2],hash[3],hash[4]);
    return nvs_get_blob(nv,key,bad,&n)==ESP_OK&&n==32&&!memcmp(bad,hash,32);
}
static esp_err_t reject_hash(const uint8_t *hash)
{
    char key[15];snprintf(key,sizeof(key),"bad%02x%02x%02x%02x%02x",hash[0],hash[1],hash[2],hash[3],hash[4]);
    esp_err_t err=nvs_set_blob(nv,key,hash,32);
    return err==ESP_OK?nvs_commit(nv):err;
}
static const char *check_header(const uint8_t *bytes,size_t length,esp_app_desc_t *desc)
{
    if(length<HEADER_SIZE)return "firmware_header_incomplete";
    esp_image_header_t image;memcpy(&image,bytes,sizeof(image));
    memcpy(desc,bytes+sizeof(image)+sizeof(esp_image_segment_header_t),sizeof(*desc));
    if(image.magic!=ESP_IMAGE_HEADER_MAGIC||image.chip_id!=ESP_CHIP_ID_ESP32P4||!image.hash_appended)return "firmware_wrong_chip";
    if(desc->magic_word!=ESP_APP_DESC_MAGIC_WORD||!memchr(desc->project_name,0,sizeof(desc->project_name))||!memchr(desc->version,0,sizeof(desc->version)))return "firmware_invalid_header";
    const esp_app_desc_t *current=esp_app_get_description();
    if(strcmp(desc->project_name,current->project_name))return "firmware_wrong_project";
    if(!memcmp(desc->app_elf_sha256,current->app_elf_sha256,32))return "firmware_already_installed";
    if(!version_number(desc->version)||version_number(desc->version)<=version_number(current->version))return "firmware_not_newer";
    if(rejected_hash(desc->app_elf_sha256))return "firmware_failed_before";
    return NULL;
}
static void abort_upload(const char *reason)
{
    if(started)esp_ota_abort(handle);
    started=false;token=0;strlcpy(phase,"error",sizeof(phase));strlcpy(detail,reason,sizeof(detail));
}
static const char *begin_upload(const uint8_t *header,size_t n,size_t size,bool from_sd)
{
    if(started||(sd_pending&&!from_sd)||reboot_at)return "firmware_busy";
    if(!healthy)return "firmware_starting";
    target=esp_ota_get_next_update_partition(NULL);
    if(!target||size<HEADER_SIZE||size>target->size)return "firmware_invalid_size";
    const char *error=check_header(header,n,&candidate);if(error)return error;
    /* Erase sectors as data arrives instead of pausing for a whole-slot erase. */
    if(esp_ota_begin(target,OTA_WITH_SEQUENTIAL_WRITES,&handle)!=ESP_OK)return "firmware_begin_failed";
    started=true;save_to_sd=false;expected=size;received=0;token=esp_random();if(!token)token=1;touched=esp_timer_get_time();
    strlcpy(phase,"uploading",sizeof(phase));detail[0]=0;return NULL;
}
/* Stage only an already verified app, using a generated path and atomic rename.
 * This also permits preparing an SD update without removing the installed card. */
static const char *save_verified_image(void)
{
    if(script_library_storage_lock()!=ESP_OK)return "no_sd_card";
    const char *error=NULL;FILE *file=NULL;uint8_t *buffer=NULL;
    char path[192],temporary[192];
    snprintf(path,sizeof(path),UPDATE_DIR "/" PREFIX "%s.bin",candidate.version);
    snprintf(temporary,sizeof(temporary),UPDATE_DIR "/.upload-%08" PRIx32 ".tmp",esp_random());
    if(mkdir(UPDATE_DIR,0777)&&errno!=EEXIST){error="firmware_directory_unavailable";goto done;}
    struct stat st;
    if(!stat(path,&st)){error="firmware_file_exists";goto done;}
    buffer=malloc(4096);file=fopen(temporary,"wb");
    if(!buffer||!file){error="firmware_write_failed";goto done;}
    for(size_t offset=0;offset<expected;){
        size_t n=expected-offset;if(n>4096)n=4096;
        if(esp_partition_read(target,offset,buffer,n)!=ESP_OK||fwrite(buffer,1,n,file)!=n){error="firmware_write_failed";break;}
        offset+=n;vTaskDelay(1);
    }
    if(!error&&(fflush(file)||fsync(fileno(file))))error="firmware_write_failed";
    if(fclose(file)&&!error)error="firmware_write_failed";
    file=NULL;
    if(!error&&rename(temporary,path))error="firmware_write_failed";
done:
    if(file)fclose(file);
    if(error)unlink(temporary);
    free(buffer);script_library_storage_unlock();
    if(!error){strlcpy(available,PREFIX,sizeof(available));strlcat(available,candidate.version,sizeof(available));strlcat(available,".bin",sizeof(available));strlcpy(phase,"idle",sizeof(phase));strlcpy(detail,"firmware_staged",sizeof(detail));}
    return error;
}
static const char *finish_upload(void)
{
    if(!started||received!=expected)return "firmware_incomplete";
    esp_err_t err=esp_ota_end(handle);started=false;token=0;
    if(err!=ESP_OK){
        reject_hash(candidate.app_elf_sha256);
        strlcpy(phase,"error",sizeof(phase));return "firmware_verification_failed";
    }
    esp_app_desc_t written;
    if(esp_ota_get_partition_description(target,&written)!=ESP_OK||
       memcmp(&written,&candidate,sizeof(written)))return "firmware_header_changed";
    if(save_to_sd)return save_verified_image();
    /* Commit attempt before changing the boot slot. The previous app detects
     * rollback and blacklists this exact build rather than retrying at boot. */
    if(nvs_set_blob(nv,"attempt",candidate.app_elf_sha256,32)!=ESP_OK||nvs_commit(nv)!=ESP_OK)return "firmware_save_failed";
    if(esp_ota_set_boot_partition(target)!=ESP_OK){nvs_erase_key(nv,"attempt");nvs_commit(nv);return "firmware_select_failed";}
    strlcpy(phase,"restarting",sizeof(phase));reboot_at=esp_timer_get_time()+2500000;return NULL;
}
static void scan_sd(bool apply)
{
    const char *error=NULL;char selected[128]="";uint64_t newest=0;esp_app_desc_t description;
    if(script_library_storage_lock()!=ESP_OK){error="no_sd_card";goto done;}
    DIR *dir=opendir(UPDATE_DIR);
    if(!dir){if(mkdir(UPDATE_DIR,0777)!=0)error="firmware_directory_unavailable";}
    if(dir){
        struct dirent *entry;
        while((entry=readdir(dir))){
            const char *name=entry->d_name;size_t n=strlen(name),prefix=strlen(PREFIX);
            if(n<=prefix+4||n>=sizeof(selected)||strncmp(name,PREFIX,prefix)||strcmp(name+n-4,".bin"))continue;
            char version[32];size_t vn=n-prefix-4;if(vn>=sizeof(version))continue;memcpy(version,name+prefix,vn);version[vn]=0;
            uint64_t number=version_number(version);if(number<=newest)continue;
            char path[192];snprintf(path,sizeof(path),UPDATE_DIR"/%s",name);FILE *f=fopen(path,"rb");if(!f)continue;
            uint8_t header[HEADER_SIZE];size_t got=fread(header,1,sizeof(header),f);fclose(f);
            if(check_header(header,got,&description)||strcmp(version,description.version))continue;
            newest=number;strlcpy(selected,name,sizeof(selected));
        }
        closedir(dir);
    }
    if(apply&&selected[0]){
        char path[192];snprintf(path,sizeof(path),UPDATE_DIR"/%s",selected);FILE *f=fopen(path,"rb");
        struct stat st;uint8_t *buffer=malloc(4096);
        if(!f||!buffer||stat(path,&st))error="firmware_read_failed";
        else {
            size_t n=fread(buffer,1,4096,f);
            /* Keep the SD reservation until the whole operation completes.
             * HTTP/BLE clients must not write or cancel this worker's handle. */
            xSemaphoreTake(lock,portMAX_DELAY);error=begin_upload(buffer,n,st.st_size,true);publish_progress();xSemaphoreGive(lock);
            bool owns_upload=!error;
            while(!error&&n){
                xSemaphoreTake(lock,portMAX_DELAY);
                if(!started||received+n>expected)error="firmware_incomplete";
                else if(esp_ota_write(handle,buffer,n)!=ESP_OK)error="firmware_write_failed";else received+=n;
                touched=esp_timer_get_time();publish_progress();xSemaphoreGive(lock);
                if(error)break;
                n=fread(buffer,1,4096,f);vTaskDelay(1);
            }
            xSemaphoreTake(lock,portMAX_DELAY);
            if(!error&&ferror(f))error="firmware_read_failed";
            if(!error)error=finish_upload();
            if(error&&owns_upload)abort_upload(error);
            publish_progress();xSemaphoreGive(lock);
        }
        free(buffer);if(f)fclose(f);
    }
    script_library_storage_unlock();
done:
    xSemaphoreTake(lock,portMAX_DELAY);sd_pending=false;
    strlcpy(available,selected,sizeof(available));
    if(error){strlcpy(phase,"error",sizeof(phase));strlcpy(detail,error,sizeof(detail));}
    else if(!reboot_at){strlcpy(phase,"idle",sizeof(phase));strlcpy(detail,selected[0]?"firmware_available":"firmware_no_new_version",sizeof(detail));}
    publish_progress();xSemaphoreGive(lock);
}
static void worker(void *arg)
{
    (void)arg;
    for(;;){
        xSemaphoreTake(lock,portMAX_DELAY);
        bool scan=sd_pending&&healthy,apply=sd_apply;
        if(started&&esp_timer_get_time()-touched>90000000)abort_upload("firmware_upload_timeout");
        bool restart=reboot_at&&esp_timer_get_time()>=reboot_at;
        if(!healthy&&esp_timer_get_time()>60000000){
            esp_ota_img_states_t state;
            if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY)
                restart=true; /* Bootloader rolls back an unconfirmed startup. */
        }
        publish_progress();xSemaphoreGive(lock);
        if(scan)scan_sd(apply);
        if(restart){teleprompter_device_action(TP_DEVICE_RESTART);vTaskDelay(pdMS_TO_TICKS(2000));esp_restart();}
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
esp_err_t firmware_update_init(void)
{
    lock=xSemaphoreCreateMutex();if(!lock)return ESP_ERR_NO_MEM;
    esp_err_t err=nvs_open("firmware",NVS_READWRITE,&nv);if(err!=ESP_OK)return err;
    uint8_t auto_value=0;nvs_get_u8(nv,"automatic",&auto_value);automatic=auto_value!=0;
    auto_value=1;nvs_get_u8(nv,"online_auto",&auto_value);online_automatic=auto_value!=0;
    if(automatic&&online_automatic){
        online_automatic=false;nvs_set_u8(nv,"online_auto",0);nvs_commit(nv);
    }
    uint8_t attempt[32];size_t length=32;
    if(nvs_get_blob(nv,"attempt",attempt,&length)==ESP_OK&&length==32&&memcmp(attempt,esp_app_get_description()->app_elf_sha256,32)){
        if(reject_hash(attempt)==ESP_OK){nvs_erase_key(nv,"attempt");nvs_commit(nv);}
        else {automatic=false;nvs_set_u8(nv,"automatic",0);nvs_commit(nv);}
        strlcpy(detail,"firmware_rolled_back",sizeof(detail));
    }
    return xTaskCreate(worker,"firmware",8192,NULL,1,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;
}
void firmware_update_healthy(void)
{
    xSemaphoreTake(lock,portMAX_DELAY);
    if(!healthy){
        esp_ota_img_states_t state;const esp_partition_t *running=esp_ota_get_running_partition();
        if(esp_ota_get_state_partition(running,&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY){
            if(esp_ota_mark_app_valid_cancel_rollback()!=ESP_OK){xSemaphoreGive(lock);return;}
        }
        nvs_erase_key(nv,"attempt");nvs_commit(nv);healthy=true;
        if(automatic){sd_pending=true;sd_apply=true;}
        ESP_LOGI(TAG,"Confirmed healthy firmware %s",esp_app_get_description()->version);
    }
    publish_progress();xSemaphoreGive(lock);
}
/* Called with the update mutex held; exclude OTA and an active SD writer. */
static const char *request_factory_reset(const cJSON *request)
{
    if(!cJSON_IsTrue(cJSON_GetObjectItem(request,"confirmed")))return "confirmation_required";
    if(started||sd_pending||reboot_at)return "firmware_busy";
    if(!healthy)return "firmware_starting";
    const char *lifecycle=teleprompter_lifecycle();
    if(strcmp(lifecycle,"ready")&&strcmp(lifecycle,"asleep"))return "firmware_busy";
    if(script_library_try_idle_lock()!=ESP_OK)return "sd_busy";
    esp_err_t err=factory_reset_prepare();
    script_library_storage_unlock();
    if(err!=ESP_OK)return "factory_reset_failed";
    strlcpy(phase,"resetting",sizeof(phase));detail[0]=0;return NULL;
}

cJSON *firmware_update_request(const cJSON *request)
{
    if(!strcmp(teleprompter_lifecycle(),"shutting_down"))return reply("firmware_busy");
    xSemaphoreTake(lock,portMAX_DELAY);
    const cJSON *action=cJSON_GetObjectItem(request,"action");const char *error=NULL;
    const char *a=cJSON_IsString(action)?action->valuestring:"status";
    cJSON *out=NULL;
    if(strcmp(a,"status")&&script_library_maintenance()){
        xSemaphoreGive(lock);return reply("sd_busy");
    }
    if(factory_reset_pending()&&strcmp(a,"status"))error="factory_reset_busy";
    else if(!strcmp(a,"factory_reset"))error=request_factory_reset(request);
    else if(!strcmp(a,"settings")){
        const cJSON *value=cJSON_GetObjectItem(request,"automatic");
        const cJSON *online_value=cJSON_GetObjectItem(request,"onlineAutomatic");
        if(started||sd_pending||reboot_at)error="firmware_busy";
        else if((!value&&!online_value)||(value&&!cJSON_IsBool(value))||(online_value&&!cJSON_IsBool(online_value)))error="invalid_request";
        else if(cJSON_IsTrue(value)&&cJSON_IsTrue(online_value))error="invalid_request";
        else {
            bool next_sd=value?cJSON_IsTrue(value):automatic;
            bool next_online=online_value?cJSON_IsTrue(online_value):online_automatic;
            if(cJSON_IsTrue(value))next_online=false;
            if(cJSON_IsTrue(online_value))next_sd=false;
            esp_err_t saved=ESP_OK;
            saved=nvs_set_u8(nv,"automatic",next_sd);
            if(saved==ESP_OK)saved=nvs_set_u8(nv,"online_auto",next_online);
            if(saved==ESP_OK)saved=nvs_commit(nv);
            if(saved!=ESP_OK)error="firmware_save_failed";
            else {automatic=next_sd;online_automatic=next_online;}
        }
    }else if(!strcmp(a,"scan")||!strcmp(a,"sd")){
        if(started||sd_pending||reboot_at)error="firmware_busy";
        else {sd_pending=true;sd_apply=!strcmp(a,"sd");strlcpy(phase,"checking",sizeof(phase));detail[0]=0;available[0]=0;}
    }else if(!strcmp(a,"begin")||!strcmp(a,"chunk")){
        const cJSON *data=cJSON_GetObjectItem(request,"data");uint8_t bytes[4096];size_t n=0;
        if(sd_pending)error="firmware_busy";
        else if(!cJSON_IsString(data)||strlen(data->valuestring)>5600||mbedtls_base64_decode(bytes,sizeof(bytes),&n,(uint8_t*)data->valuestring,strlen(data->valuestring)))error="firmware_invalid_data";
        else if(!strcmp(a,"begin")){
            const cJSON *size=cJSON_GetObjectItem(request,"size");
            const cJSON *destination=cJSON_GetObjectItem(request,"destination");
            if(destination&&(!cJSON_IsString(destination)||strcmp(destination->valuestring,"sd")))error="invalid_request";
            else if(!cJSON_IsNumber(size)||size->valuedouble!=size->valueint||size->valueint<=0)error="firmware_invalid_size";
            else {error=begin_upload(bytes,n,size->valueint,false);if(!error)save_to_sd=destination!=NULL;}
        }else{
            const cJSON *id=cJSON_GetObjectItem(request,"token"),*offset=cJSON_GetObjectItem(request,"offset");
            if(!started||!cJSON_IsNumber(id)||id->valuedouble!=token||!cJSON_IsNumber(offset)||offset->valuedouble!=received||!n||received+n>expected)error="firmware_invalid_offset";
            else if(!received&&(n<HEADER_SIZE||memcmp(bytes+sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t),&candidate,sizeof(candidate))))error="firmware_header_changed";
            else if(esp_ota_write(handle,bytes,n)!=ESP_OK){abort_upload("firmware_write_failed");error=detail;}
            else {received+=n;touched=esp_timer_get_time();}
        }
    }else if(!strcmp(a,"finish")||!strcmp(a,"cancel")){
        const cJSON *id=cJSON_GetObjectItem(request,"token");
        if(sd_pending)error="firmware_busy";
        else if(!started||!cJSON_IsNumber(id)||id->valuedouble!=token)error="firmware_invalid_session";
        else if(!strcmp(a,"cancel")){abort_upload("firmware_cancelled");}
        else {error=finish_upload();if(error)abort_upload(error);}
    }else if(strcmp(a,"status"))error="invalid_request";
    out=reply(error);
    cJSON_AddStringToObject(out,"version",esp_app_get_description()->version);
    cJSON_AddStringToObject(out,"phase",phase);cJSON_AddStringToObject(out,"detail",detail);
    cJSON_AddStringToObject(out,"available",available);cJSON_AddBoolToObject(out,"automatic",automatic);
    cJSON_AddBoolToObject(out,"onlineAutomatic",online_automatic);
    cJSON_AddBoolToObject(out,"factoryResetSupported",true);
    cJSON_AddBoolToObject(out,"healthy",healthy);
    cJSON_AddNumberToObject(out,"received",received);cJSON_AddNumberToObject(out,"size",expected);cJSON_AddNumberToObject(out,"token",token);
    publish_progress();xSemaphoreGive(lock);return out;
}
