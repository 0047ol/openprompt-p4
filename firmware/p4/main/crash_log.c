/* Crash capture is intentionally independent of SD, LVGL and the log driver.
 * No allocation, flash write, locks or arbitrary strings in the panic path.
 * IDF's original panic handler still handles the exception and reset. */
#include "crash_log.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "esp_attr.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_private/panic_internal.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "riscv/rvruntime-frames.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_sig_map.h"

#define CRASH_MAGIC 0x43525034u
#define LIVE_MAGIC 0x43524c34u
#define HISTORY_MAGIC 0x43524831u
#define HISTORY_SIZE 4
#define CODE_SAMPLES 12
typedef struct { uint32_t code, inverse, ms; } breadcrumb_t;
typedef struct {
    uint32_t id, boot, reset, uptime_ms, captured, core, exception, pc, ra, sp, cause, value;
    breadcrumb_t stages[TP_TRACE_COUNT];
    uint32_t candidates[CODE_SAMPLES];
    char version[32];
} crash_record_t;
typedef struct { uint32_t ready, signature; crash_record_t record; } retained_t;
typedef struct {
    uint32_t magic, sequence, acknowledged, count;
    crash_record_t records[HISTORY_SIZE];
} history_t;
static RTC_NOINIT_ATTR volatile retained_t retained;
static crash_record_t incoming;
static history_t history;
static nvs_handle_t handle;
static SemaphoreHandle_t mutex;
static atomic_uint last_id, count, acknowledged;
static bool current_abnormal;
extern char _iram_text_start[],_iram_text_end[],_instruction_reserved_start[],_instruction_reserved_end[];

static uint32_t IRAM_ATTR checksum(const volatile crash_record_t *record)
{
    const volatile unsigned char *p=(const volatile unsigned char *)record;
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<sizeof(*record);++i)hash=(hash^p[i])*16777619u;
    return hash;
}

/* Called after IDF stalls the other CPU. Bypass LEDC/driver locks, replace
 * PWM's GPIO-matrix source with a low GPIO level and hold it across reset. */
static void IRAM_ATTR panic_dark(void)
{
    gpio_ll_set_level(&GPIO,20,0);
    gpio_ll_set_output_signal_matrix_source(&GPIO,20,SIG_GPIO_OUT_IDX,false);
    gpio_ll_set_output_enable_ctrl(&GPIO,20,false,false);
    gpio_ll_output_enable(&GPIO,20);
    gpio_ll_hold_en(&GPIO,20);
}

extern void __real_esp_panic_handler(panic_info_t *info);
void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t *info)
{
    panic_dark();
    retained.ready=0;
    volatile crash_record_t *r=&retained.record;
    r->captured=1;r->core=info->core;r->exception=g_panic_abort?PANIC_EXCEPTION_ABORT:info->exception;
    r->pc=(uint32_t)(uintptr_t)info->addr;
    const RvExcFrame *f=info->frame;
    if(f&&esp_ptr_in_dram(f)&&esp_ptr_in_dram((const unsigned char *)f+sizeof(*f)-1)){
        r->ra=f->ra;r->sp=f->sp;r->cause=f->mcause;r->value=f->mtval;
        /* These are possible CODE addresses, not a claimed unwind. Never
         * persist raw stack contents, which can contain user data. */
        unsigned used=0;
        uintptr_t at=(uintptr_t)f->sp;
        for(unsigned i=0;i<256&&used<CODE_SAMPLES&&!(at&3u);++i,at+=4){
            if(!esp_ptr_in_dram((void *)at)||!esp_ptr_in_dram((void *)(at+3)))break;
            uint32_t word=*(const volatile uint32_t *)at;
            if((word>=(uintptr_t)_instruction_reserved_start&&word<(uintptr_t)_instruction_reserved_end)||
               (word>=(uintptr_t)_iram_text_start&&word<(uintptr_t)_iram_text_end))r->candidates[used++]=word;
        }
    }
    retained.signature=checksum(r);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    retained.ready=CRASH_MAGIC;
    __real_esp_panic_handler(info);
}

static bool abnormal(esp_reset_reason_t reason)
{
    return reason==ESP_RST_PANIC||reason==ESP_RST_INT_WDT||reason==ESP_RST_TASK_WDT||
           reason==ESP_RST_WDT||reason==ESP_RST_BROWNOUT;
}
void crash_log_early_init(void)
{
    esp_reset_reason_t reason=esp_reset_reason();
    current_abnormal=abnormal(reason);
    bool captured=current_abnormal&&retained.ready==CRASH_MAGIC&&retained.signature==checksum(&retained.record);
    memset(&incoming,0,sizeof(incoming));
    if(captured||(current_abnormal&&retained.ready==LIVE_MAGIC)){
        const volatile unsigned char *p=(const volatile unsigned char *)&retained.record;
        for(unsigned i=0;i<sizeof(incoming);++i)((unsigned char *)&incoming)[i]=p[i];
    }
    incoming.version[sizeof(incoming.version)-1]=0;
    incoming.reset=reason;
    /* Commit only after NVS is initialized. In a boot loop the previous
     * captured record remains available until this early copy is complete. */
    retained.ready=0;
    crash_record_t fresh={.boot=esp_random()};
    snprintf(fresh.version,sizeof(fresh.version),"%s",esp_app_get_description()->version);
    const unsigned char *p=(const unsigned char *)&fresh;
    for(unsigned i=0;i<sizeof(fresh);++i)((volatile unsigned char *)&retained.record)[i]=p[i];
    retained.ready=LIVE_MAGIC;
    crash_log_stage(TP_TRACE_BOOT,1);
}

void crash_log_stage(tp_trace_domain_t domain,unsigned stage)
{
    if((unsigned)domain>=TP_TRACE_COUNT)return;
    volatile breadcrumb_t *b=&retained.record.stages[domain];
    b->inverse=0;b->ms=(uint32_t)(esp_timer_get_time()/1000);b->code=stage;
    __atomic_thread_fence(__ATOMIC_RELEASE);b->inverse=~stage;
    /* Per-domain time avoids a global contended live counter. */
}

void crash_log_init(void)
{
    mutex=xSemaphoreCreateMutex();
    if(!mutex)return;
    if(nvs_open("tp_crash",NVS_READWRITE,&handle)==ESP_OK){
        size_t size=sizeof(history);
        if(nvs_get_blob(handle,"history",&history,&size)!=ESP_OK||size!=sizeof(history)||history.magic!=HISTORY_MAGIC||history.count>HISTORY_SIZE){
            memset(&history,0,sizeof(history));history.magic=HISTORY_MAGIC;
        }
    }
    if(current_abnormal){
        if(!incoming.version[0])snprintf(incoming.version,sizeof(incoming.version),"%s",esp_app_get_description()->version);
        for(unsigned i=0;i<TP_TRACE_COUNT;++i){
            breadcrumb_t *b=&incoming.stages[i];
            if(b->inverse!=~b->code)memset(b,0,sizeof(*b));
            if(b->ms>incoming.uptime_ms)incoming.uptime_ms=b->ms;
        }
        incoming.id=++history.sequence;
        if(!incoming.id)incoming.id=++history.sequence;
        memmove(history.records+1,history.records,sizeof(history.records[0])*(HISTORY_SIZE-1));
        history.records[0]=incoming;if(history.count<HISTORY_SIZE)++history.count;
        if(handle){
            esp_err_t err=nvs_set_blob(handle,"history",&history,sizeof(history));
            if(err==ESP_OK)err=nvs_commit(handle);
            ESP_LOGW("tp_crash","abnormal reset=%u capture=%u pc=%08lx persistence=%s",incoming.reset,incoming.captured,(unsigned long)incoming.pc,esp_err_to_name(err));
        }
    }
    atomic_store(&last_id,history.sequence);atomic_store(&count,history.count);atomic_store(&acknowledged,history.acknowledged);
}
bool crash_log_pending(void){return atomic_load(&count)&&atomic_load(&last_id)!=atomic_load(&acknowledged);}
unsigned crash_log_last_id(void){return atomic_load(&last_id);}
unsigned crash_log_count(void){return atomic_load(&count);}

static const char *reset_name(unsigned reason)
{
    switch(reason){case ESP_RST_PANIC:return "panic";case ESP_RST_INT_WDT:return "interrupt_watchdog";
    case ESP_RST_TASK_WDT:return "task_watchdog";case ESP_RST_WDT:return "watchdog";
    case ESP_RST_BROWNOUT:return "brownout";default:return "unknown";}
}
cJSON *crash_log_request(const cJSON *request)
{
    cJSON *out=cJSON_CreateObject();if(!out)return NULL;
    if(!mutex){cJSON_AddBoolToObject(out,"ok",false);cJSON_AddStringToObject(out,"error","diagnostics_unavailable");return out;}
    xSemaphoreTake(mutex,portMAX_DELAY);
    const cJSON *action=cJSON_GetObjectItem(request,"action");
    bool ack=cJSON_IsString(action)&&!strcmp(action->valuestring,"acknowledge");
    if(ack){
        const cJSON *id=cJSON_GetObjectItem(request,"id");
        /* An older browser may acknowledge only the fault it actually saw. */
        if(!cJSON_IsNumber(id)||id->valuedouble!=history.sequence){cJSON_AddBoolToObject(out,"ok",false);cJSON_AddStringToObject(out,"error","diagnostics_changed");goto done;}
        uint32_t previous=history.acknowledged;history.acknowledged=history.sequence;
        esp_err_t err=handle?nvs_set_blob(handle,"history",&history,sizeof(history)):ESP_FAIL;
        if(err==ESP_OK)err=nvs_commit(handle);
        if(err!=ESP_OK){history.acknowledged=previous;cJSON_AddBoolToObject(out,"ok",false);cJSON_AddStringToObject(out,"error","diagnostics_save_failed");goto done;}
        atomic_store(&acknowledged,history.acknowledged);
    }else if(cJSON_IsString(action)&&strcmp(action->valuestring,"status")){
        cJSON_AddBoolToObject(out,"ok",false);cJSON_AddStringToObject(out,"error","invalid_action");goto done;
    }
    cJSON_AddBoolToObject(out,"ok",true);cJSON_AddNumberToObject(out,"lastId",history.sequence);
    cJSON_AddBoolToObject(out,"pending",crash_log_pending());
    cJSON_AddBoolToObject(out,"persistent",handle!=0);
    cJSON_AddStringToObject(out,"scope","reset summary, code addresses and fixed operation stages; not a full core dump");
    cJSON *records=cJSON_AddArrayToObject(out,"records");
    static const char *const domains[]={"boot","storage","update","power","sd_test"};
    for(unsigned i=0;i<history.count;++i){
        const crash_record_t *r=&history.records[i];cJSON *item=cJSON_CreateObject();cJSON_AddItemToArray(records,item);
        cJSON_AddNumberToObject(item,"id",r->id);cJSON_AddStringToObject(item,"version",r->version);
        cJSON_AddStringToObject(item,"reason",reset_name(r->reset));cJSON_AddNumberToObject(item,"resetReason",r->reset);
        cJSON_AddBoolToObject(item,"panicCaptured",r->captured);cJSON_AddNumberToObject(item,"uptimeLowerBoundMs",r->uptime_ms);
        if(r->captured){
            cJSON_AddNumberToObject(item,"core",r->core);cJSON_AddNumberToObject(item,"exception",r->exception);
            char hex[12];
#define ADD_ADDRESS(name,field) snprintf(hex,sizeof(hex),"0x%08lx",(unsigned long)r->field);cJSON_AddStringToObject(item,name,hex)
            ADD_ADDRESS("pc",pc);ADD_ADDRESS("returnAddress",ra);ADD_ADDRESS("stackPointer",sp);ADD_ADDRESS("machineCause",cause);ADD_ADDRESS("faultAddress",value);
#undef ADD_ADDRESS
            cJSON *codes=cJSON_AddArrayToObject(item,"possibleCodeAddresses");
            for(unsigned n=0;n<CODE_SAMPLES&&r->candidates[n];++n){snprintf(hex,sizeof(hex),"0x%08lx",(unsigned long)r->candidates[n]);cJSON_AddItemToArray(codes,cJSON_CreateString(hex));}
        }
        cJSON *stages=cJSON_AddObjectToObject(item,"stages");
        for(unsigned n=0;n<TP_TRACE_COUNT;++n)if(r->stages[n].inverse==~r->stages[n].code){
            cJSON *stage=cJSON_AddObjectToObject(stages,domains[n]);cJSON_AddNumberToObject(stage,"code",r->stages[n].code);cJSON_AddNumberToObject(stage,"atMs",r->stages[n].ms);
        }
    }
done:
    xSemaphoreGive(mutex);return out;
}
