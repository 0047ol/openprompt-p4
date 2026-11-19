#include "web_assets.h"
#include "web_asset_tags.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include "reader_shards.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"

static SemaphoreHandle_t s_asset_slots;
typedef struct { httpd_req_t *request; tp_web_asset_t asset; } asset_job_t;
static bool editor_not_modified(httpd_req_t *req, esp_err_t *result);
bool web_assets_busy(void){return s_asset_slots&&uxSemaphoreGetCount(s_asset_slots)<2;}

esp_err_t web_assets_init(void)
{
    if(!s_asset_slots)s_asset_slots=xSemaphoreCreateCounting(2,2);
    return s_asset_slots?ESP_OK:ESP_ERR_NO_MEM;
}

static void asset_send_task(void *context)
{
    asset_job_t *job=context;
    httpd_req_t *req=job->request;
    /* These sockets run outside the HTTP control task. Allow a bounded
     * radio stall without truncating an otherwise healthy page/font body. */
    const struct timeval transfer_timeout={.tv_sec=15};
    setsockopt(httpd_req_to_sockfd(req),SOL_SOCKET,SO_SNDTIMEO,&transfer_timeout,sizeof(transfer_timeout));
    int64_t started=esp_timer_get_time();
    esp_err_t result=web_asset_send(req,job->asset);
    if(result!=ESP_OK){
        ESP_LOGW("tp_assets","transfer asset=%d duration_ms=%lld error=%s",job->asset,
                 (long long)((esp_timer_get_time()-started)/1000),esp_err_to_name(result));
        httpd_sess_trigger_close(req->handle,httpd_req_to_sockfd(req));
    }else if(esp_timer_get_time()-started>1000000){
        ESP_LOGW("tp_assets","slow transfer asset=%d duration_ms=%lld",job->asset,
                 (long long)((esp_timer_get_time()-started)/1000));
    }
    const struct timeval normal_timeout={.tv_sec=5};
    setsockopt(httpd_req_to_sockfd(req),SOL_SOCKET,SO_SNDTIMEO,&normal_timeout,sizeof(normal_timeout));
    httpd_req_async_handler_complete(req);
    free(job);
    xSemaphoreGive(s_asset_slots);
    vTaskDelete(NULL);
}

esp_err_t web_asset_send_async(httpd_req_t *req,tp_web_asset_t asset)
{
    /* A cached controller has no body to transfer. Revalidate immediately,
     * even when slow downloads occupy both background workers. */
    esp_err_t cached;
    if(asset!=TP_ASSET_FONT && editor_not_modified(req,&cached))return cached;
    /* Portal, controller and offline fonts share one bounded transfer pool.
     * A stalled body must not run on the HTTP control/WebSocket task. */
    if(!s_asset_slots)return ESP_ERR_INVALID_STATE;
    if(xSemaphoreTake(s_asset_slots,0)!=pdTRUE){
        httpd_resp_set_status(req,"503 Service Unavailable");
        httpd_resp_set_hdr(req,"Retry-After","2");
        return httpd_resp_sendstr(req,"Asset transfers busy; retry shortly");
    }
    asset_job_t *job=calloc(1,sizeof(*job));
    if(!job){xSemaphoreGive(s_asset_slots);return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No asset worker memory");}
    esp_err_t result=httpd_req_async_handler_begin(req,&job->request);
    if(result!=ESP_OK){free(job);xSemaphoreGive(s_asset_slots);return result;}
    job->asset=asset;
    /* Lower priority than display_ui: downloads must not preempt scrolling. */
    if(xTaskCreate(asset_send_task,"tp_asset",4096,job,1,NULL)!=pdPASS){
        httpd_resp_send_err(job->request,HTTPD_500_INTERNAL_SERVER_ERROR,"No asset worker memory");
        httpd_req_async_handler_complete(job->request);
        free(job);xSemaphoreGive(s_asset_slots);
    }
    return ESP_OK;
}

#define ASSET_SYMBOL(name) \
    extern const unsigned char name##_start[] asm("_binary_" #name "_start"); \
    extern const unsigned char name##_end[] asm("_binary_" #name "_end")
ASSET_SYMBOL(web_editor_html);
ASSET_SYMBOL(web_editor_html_gz);
ASSET_SYMBOL(reader_shards_bin);


static bool accepts_gzip(httpd_req_t *req)
{
    char header[512];
    if(httpd_req_get_hdr_value_str(req,"Accept-Encoding",header,sizeof(header))!=ESP_OK)return false;
    bool wildcard=false;
    char *save=NULL;
    for(char *part=strtok_r(header,",",&save);part;part=strtok_r(NULL,",",&save)){
        while(isspace((unsigned char)*part))++part;
        char *parameter=strchr(part,';');
        if(parameter)*parameter++=0;
        char *end=part+strlen(part);
        while(end>part&&isspace((unsigned char)end[-1]))*--end=0;
        double quality=1;
        if(parameter){
            while(isspace((unsigned char)*parameter))++parameter;
            if(!strncasecmp(parameter,"q=",2))quality=strtod(parameter+2,NULL);
        }
        if(!strcasecmp(part,"gzip"))return quality>0;
        if(!strcmp(part,"*"))wildcard=quality>0;
    }
    return wildcard;
}

static bool matches_etag(httpd_req_t *req,const char *etag)
{
    char header[512];
    if(httpd_req_get_hdr_value_str(req,"If-None-Match",header,sizeof(header))!=ESP_OK)return false;
    char *save=NULL;
    for(char *part=strtok_r(header,",",&save);part;part=strtok_r(NULL,",",&save)){
        while(isspace((unsigned char)*part))++part;
        if(!strncmp(part,"W/",2))part+=2;
        char *end=part+strlen(part);
        while(end>part&&isspace((unsigned char)end[-1]))*--end=0;
        if(!strcmp(part,"*")||!strcmp(part,etag))return true;
    }
    return false;
}

static void editor_headers(httpd_req_t *req,bool gzip,const char *etag)
{
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-cache");
    httpd_resp_set_hdr(req,"ETag",etag);
    httpd_resp_set_hdr(req,"Vary","Accept-Encoding");
    if(gzip)httpd_resp_set_hdr(req,"Content-Encoding","gzip");
}

static bool editor_not_modified(httpd_req_t *req,esp_err_t *result)
{
    bool gzip=accepts_gzip(req);
    const char *etag=gzip?TP_EDITOR_GZIP_ETAG:TP_EDITOR_IDENTITY_ETAG;
    if(!matches_etag(req,etag))return false;
    editor_headers(req,gzip,etag);
    httpd_resp_set_status(req,"304 Not Modified");
    *result=httpd_resp_send(req,NULL,0);
    return true;
}

esp_err_t web_asset_send(httpd_req_t *req,tp_web_asset_t asset)
{
    if(asset==TP_ASSET_FONT){
        char query[40],value[16];
        if(httpd_req_get_url_query_str(req,query,sizeof(query))!=ESP_OK ||
           httpd_query_key_value(query,"shard",value,sizeof(value))!=ESP_OK)
            return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Missing font shard");
        char *tail;unsigned long index=strtoul(value,&tail,10);
        if(!value[0]||*tail||index>=sizeof(reader_shards)/sizeof(reader_shards[0]))
            return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"Unknown font shard");
        httpd_resp_set_type(req,"font/woff2");
        httpd_resp_set_hdr(req,"Cache-Control","public, max-age=86400");
        httpd_resp_set_hdr(req,"ETag",reader_shards[index].etag);
        if(matches_etag(req,reader_shards[index].etag)){
            httpd_resp_set_status(req,"304 Not Modified");return httpd_resp_send(req,NULL,0);
        }
        return httpd_resp_send(req,(const char*)reader_shards_bin_start+reader_shards[index].offset,reader_shards[index].length);
    }
    bool gzip=accepts_gzip(req);
    const unsigned char *start=gzip?web_editor_html_gz_start:web_editor_html_start;
    const unsigned char *end=gzip?web_editor_html_gz_end:web_editor_html_end-1;
    const char *etag=gzip?TP_EDITOR_GZIP_ETAG:TP_EDITOR_IDENTITY_ETAG;
    bool unchanged=matches_etag(req,etag);
    // Persist static bytes, but revalidate HTML so firmware updates never keep
    // an obsolete controller. State and credentials remain separate no-store APIs.
    editor_headers(req,gzip,etag);
    if(unchanged){httpd_resp_set_status(req,"304 Not Modified");return httpd_resp_send(req,NULL,0);}
    return httpd_resp_send(req,(const char *)start,end-start);
}
