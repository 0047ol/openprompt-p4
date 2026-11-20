/* Modified 2026-11-20: on-demand completed-frame screenshots. */
#include "display_capture.h"
#include "display_port.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static atomic_bool s_busy;
static void put32(uint8_t *p,uint32_t value)
{
    for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(value>>(8*i));
}

static esp_err_t send_capture(httpd_req_t *req)
{
    unsigned scale=2;char query[32],value[8];
    if(httpd_req_get_url_query_str(req,query,sizeof(query))==ESP_OK&&
       httpd_query_key_value(query,"scale",value,sizeof(value))==ESP_OK){
        if(!strcmp(value,"1"))scale=1;
        else if(!strcmp(value,"2"))scale=2;
        else if(!strcmp(value,"4"))scale=4;
        else return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Capture scale must be 1, 2 or 4");
    }
    tp_display_capture_t image={0};
    esp_err_t result=display_port_capture(&image,3000);
    if(result!=ESP_OK){
        httpd_resp_set_status(req,"503 Service Unavailable");
        return httpd_resp_sendstr(req,result==ESP_ERR_TIMEOUT?"Display capture timed out":"Display not ready or capture memory unavailable");
    }
    const bool landscape=image.rotation==90||image.rotation==270;
    unsigned width=(landscape?image.height:image.width)/scale,height=(landscape?image.width:image.height)/scale;
    /* Top-down BMP, BI_BITFIELDS RGB565. The native width and height are even,
     * so rows need no extra four-byte padding. One small reusable row batch
     * keeps conversion/transmission outside the UI task. */
    uint8_t header[66]={0};header[0]='B';header[1]='M';
    put32(header+2,66+width*height*2);put32(header+10,66);put32(header+14,40);
    put32(header+18,width);put32(header+22,(uint32_t)-(int32_t)height);
    header[26]=1;header[28]=16;put32(header+30,3);put32(header+34,width*height*2);
    put32(header+54,0xf800);put32(header+58,0x07e0);put32(header+62,0x001f);
    enum{ROWS=32};
    uint16_t *rows=heap_caps_malloc(width*ROWS*sizeof(uint16_t),MALLOC_CAP_SPIRAM);
    if(!rows){free(image.pixels);return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Capture row memory unavailable");}
    char info[128];snprintf(info,sizeof(info),"frame=%lu; captured-us=%lld; rotation=%u; asleep=%u; scale=%u",
        (unsigned long)image.frame,(long long)image.captured_us,image.rotation,image.asleep,scale);
    httpd_resp_set_type(req,"image/bmp");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"X-P4-Capture",info);
    httpd_resp_set_hdr(req,"Content-Disposition","inline; filename=p4-screen.bmp");
    result=httpd_resp_send_chunk(req,(const char *)header,sizeof(header));
    for(unsigned y=0;y<height&&result==ESP_OK;y+=ROWS){
        unsigned count=height-y<ROWS?height-y:ROWS;
        for(unsigned row=0;row<count;++row)for(unsigned x=0;x<width;++x){
            unsigned lx=x*scale,ly=(y+row)*scale,sx=lx,sy=ly;
            if(image.rotation==90){sx=ly;sy=image.height-1-lx;}
            else if(image.rotation==180){sx=image.width-1-lx;sy=image.height-1-ly;}
            else if(image.rotation==270){sx=image.width-1-ly;sy=lx;}
            rows[row*width+x]=image.pixels[sy*image.width+sx];
        }
        result=httpd_resp_send_chunk(req,(const char *)rows,width*count*sizeof(uint16_t));
    }
    if(result==ESP_OK)result=httpd_resp_send_chunk(req,NULL,0);
    free(rows);free(image.pixels);return result;
}

static void capture_task(void *context)
{
    httpd_req_t *req=context;
    const struct timeval timeout={.tv_sec=5};
    setsockopt(httpd_req_to_sockfd(req),SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    if(send_capture(req)!=ESP_OK)httpd_sess_trigger_close(req->handle,httpd_req_to_sockfd(req));
    httpd_req_async_handler_complete(req);atomic_store(&s_busy,false);vTaskDelete(NULL);
}

esp_err_t display_capture_get(httpd_req_t *req)
{
    if(atomic_exchange(&s_busy,true)){
        httpd_resp_set_status(req,"503 Service Unavailable");
        httpd_resp_set_hdr(req,"Retry-After","2");return httpd_resp_sendstr(req,"One screenshot is already in progress");
    }
    httpd_req_t *async=NULL;
    esp_err_t result=httpd_req_async_handler_begin(req,&async);
    if(result!=ESP_OK){atomic_store(&s_busy,false);return result;}
    if(xTaskCreate(capture_task,"tp_capture",4096,async,1,NULL)!=pdPASS){
        httpd_resp_send_err(async,HTTPD_500_INTERNAL_SERVER_ERROR,"No capture worker memory");
        httpd_req_async_handler_complete(async);atomic_store(&s_busy,false);
    }
    return ESP_OK;
}
