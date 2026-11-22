#include "factory_reset.h"
#include <stdatomic.h>
#include "nvs.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static atomic_bool pending;
bool factory_reset_pending(void){return atomic_load(&pending);}

static esp_err_t clear_namespace(const char *name)
{
    nvs_handle_t handle;
    esp_err_t err=nvs_open(name,NVS_READWRITE,&handle);
    if(err!=ESP_OK)return err;
    err=nvs_erase_all(handle);
    if(err==ESP_OK)err=nvs_commit(handle);
    nvs_close(handle);return err;
}

static esp_err_t write_marker(bool value)
{
    nvs_handle_t handle;
    esp_err_t err=nvs_open("tp_reset",NVS_READWRITE,&handle);
    if(err!=ESP_OK)return err;
    err=nvs_set_u8(handle,"pending",value);
    if(err==ESP_OK)err=nvs_commit(handle);
    nvs_close(handle);return err;
}

esp_err_t factory_reset_apply_pending(void)
{
    nvs_handle_t handle;uint8_t value=0;
    esp_err_t err=nvs_open("tp_reset",NVS_READONLY,&handle);
    if(err==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;
    if(err!=ESP_OK)return err;
    err=nvs_get_u8(handle,"pending",&value);nvs_close(handle);
    if(err==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;
    if(err!=ESP_OK||!value)return err;
    /* Idempotent: a power failure resumes the reset before loading old settings. */
    const char *names[]={"tp_device","tp_display","tp_wifi","tp_remote","nimble_bond"};
    for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);++i){
        err=clear_namespace(names[i]);if(err!=ESP_OK)return err;
    }
    /* Keep rejected-image hashes / rollback records, only reset preferences. */
    err=nvs_open("firmware",NVS_READWRITE,&handle);
    if(err!=ESP_OK)return err;
    const char *keys[]={"automatic","online_auto"};
    for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);++i){
        err=nvs_erase_key(handle,keys[i]);
        if(err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;
        if(err!=ESP_OK)break;
    }
    if(err==ESP_OK)err=nvs_commit(handle);
    nvs_close(handle);
    return err==ESP_OK?write_marker(false):err;
}

static void reset_worker(void *unused)
{
    (void)unused;
    /* Allow the HTTP/BLE reply and the display notice to arrive before reboot. */
    vTaskDelay(pdMS_TO_TICKS(1500));esp_restart();
    vTaskDelete(NULL);
}

esp_err_t factory_reset_prepare(void)
{
    bool expected=false;
    if(!atomic_compare_exchange_strong(&pending,&expected,true))return ESP_ERR_INVALID_STATE;
    esp_err_t err=write_marker(true);
    if(err==ESP_OK&&xTaskCreate(reset_worker,"factory_reset",3072,NULL,3,NULL)!=pdPASS){
        esp_err_t rollback=write_marker(false);
        if(rollback!=ESP_OK){
            /* Do not re-enable writes when a durable reset marker remains. */
            esp_restart();return rollback;
        }
        err=ESP_ERR_NO_MEM;
    }
    if(err!=ESP_OK)atomic_store(&pending,false);
    return err;
}
