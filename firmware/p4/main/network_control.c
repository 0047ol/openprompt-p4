#include "network_control.h"
#include "control_session.h"
#include "firmware_update.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "display_port.h"
#include "nvs_flash.h"
#include "prompter_state.h"
#include "app_control.h"
#include "wifi_settings.h"
#include "captive_portal.h"
#include "web_assets.h"
#include "device_settings.h"
#include "script_library.h"
#include "cJSON.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include "lwip/sockets.h"
#include <errno.h>

/* This board connects C6 to the GPIO-matrix pins, not slot 0's fixed pins. */
#if CONFIG_ESP_HOSTED_SDIO_SLOT != 1
#error "This P4 board requires SDMMC slot 1 for C6; slot 0 is fixed to the TF-card pins."
#endif

static const char *TAG = "tp_network";
static httpd_handle_t s_server;
static esp_err_t asset_get(httpd_req_t *req)
{
    return web_asset_send_async(req,!strncmp(req->uri,"/reader.woff2",13)?TP_ASSET_FONT:TP_ASSET_EDITOR);
}
static esp_err_t library_post(httpd_req_t *req)
{
    if(!control_session_http(req))return ESP_OK;
    if(req->content_len<=0||req->content_len>57344)return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid request size");
    char *body=malloc(req->content_len+1);if(!body)return ESP_ERR_NO_MEM;
    int n=0;while(n<req->content_len){int got=httpd_req_recv(req,body+n,req->content_len-n);if(got<=0){free(body);return ESP_FAIL;}n+=got;}
    body[n]=0;if(memchr(body,0,n)){free(body);return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid request");}
    char *response=script_library_request(body,TP_REQUEST_HTTP);
    cJSON *message=cJSON_Parse(body);
    const cJSON *op=cJSON_GetObjectItem(message,"op"),*client=cJSON_GetObjectItem(message,"client");
    if(cJSON_IsString(op)&&!strcmp(op->valuestring,"presence")&&cJSON_IsString(client))control_session_route_presence(req,client->valuestring);
    cJSON_Delete(message);free(body);
    if(!response)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_sendstr(req,response);free(response);return err;
}

static void status_body(char *body,size_t capacity)
{
    prompter_status_t status;
    prompter_snapshot(&status,NULL);
    char address[16];wifi_settings_address(address,sizeof(address));
    int n=prompter_status_json(body,capacity,&status);
    if(n<=0 || n>=(int)capacity){snprintf(body,capacity,"{}");return;}
    snprintf(body+n-1,capacity-n+1,",\"ok\":true,\"service\":\"teleprompter-p4\","
             "\"displayReady\":%s,\"powerRecovery\":%s,\"displayRetryAvailable\":%s,\"ip\":\"%s\",\"presenceHeartbeat\":true}",
             display_port_get_lvgl_display()?"true":"false",
             esp_reset_reason()==ESP_RST_BROWNOUT&&!display_port_get_lvgl_display()?"true":"false",
             teleprompter_display_can_retry()?"true":"false",address);
}

static esp_err_t status_get(httpd_req_t *req)
{
    if(!control_session_http(req))return ESP_OK;
    char query[64],client[33] = {0};
    if(httpd_req_get_url_query_str(req,query,sizeof(query))==ESP_OK &&
       httpd_query_key_value(query,"client",client,sizeof(client))==ESP_OK)
        device_settings_client_seen(client);
    char body[1280];
    status_body(body,sizeof(body));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t result = httpd_resp_sendstr(req, body);
    if (result == ESP_OK) wifi_settings_confirm_lan(req, client);
    return result;
}

static esp_err_t script_get(httpd_req_t *req)
{
    if(!control_session_http(req))return ESP_OK;
    char *text = malloc(PROMPTER_MAX_BYTES + 1);
    if (!text) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    prompter_status_t status;
    prompter_snapshot(&status, text);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, text, status.length);
    free(text);
    return err;
}

static esp_err_t display_retry_post(httpd_req_t *req)
{
    if(!control_session_http(req))return ESP_OK;
    if(firmware_update_in_progress()){
        httpd_resp_set_status(req,"409 Conflict");
        return httpd_resp_sendstr(req,"firmware_busy");
    }
    esp_err_t err = teleprompter_start_display();
    if (err == ESP_ERR_INVALID_STATE) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "Display startup already attempted this boot");
    }
    if (err != ESP_OK) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    ESP_LOGI(TAG, "DISPLAY_RETRY requested over HTTP; keeping Wi-Fi and USB active");
    httpd_resp_set_status(req, "202 Accepted");
    return status_get(req);
}

static esp_err_t script_put(httpd_req_t *req)
{
    if(!control_session_http(req))return ESP_OK;
    /* Older cached controllers must not replace RAM independently of the SD
     * document, its version token and its currently selected style. */
    httpd_resp_set_status(req,"410 Gone");
    return httpd_resp_sendstr(req,"Use /api/library to save and select an SD document");
}

static bool query_int(const char *query, const char *key, int *value, unsigned *mask, unsigned bit)
{
    char text[20];
    esp_err_t err = httpd_query_key_value(query, key, text, sizeof(text));
    if (err == ESP_ERR_NOT_FOUND) return true;
    if (err != ESP_OK || !text[0]) return false;
    char *end;
    long parsed = strtol(text, &end, 10);
    if (*end || parsed < -1000 || parsed > 0xffffff) return false;
    *value = (int)parsed;
    *mask |= bit;
    return true;
}

static esp_err_t apply_query(const char *query)
{
    unsigned mask = 0;
    int playing = 0, speed = 0, font = 0, rewind = 0, rotation = 0, brightness = 0;
    int letter_spacing = 0, line_spacing = 0, countdown = 0;
    int loop=0, auto_next=0, bold=0, sight_guide=0, center_highlight=0, color=0;
    prompter_status_t values={0};
    if (!query_int(query, "playing", &playing, &mask, TP_PLAYING) ||
        !query_int(query, "shuttle", &values.shuttle, &mask, TP_SHUTTLE) ||
        !query_int(query, "speed", &speed, &mask, TP_SPEED) ||
        !query_int(query, "font", &font, &mask, TP_FONT) ||
        !query_int(query, "rewind", &rewind, &mask, TP_REWIND) ||
        !query_int(query, "rotation", &rotation, &mask, TP_ROTATION) ||
        !query_int(query, "brightness", &brightness, &mask, TP_BRIGHTNESS) ||
          !query_int(query, "letterSpacing", &letter_spacing, &mask, TP_LETTER_SPACING) ||
          !query_int(query, "lineSpacing", &line_spacing, &mask, TP_LINE_SPACING) ||
          !query_int(query, "countdown", &countdown, &mask, TP_COUNTDOWN) ||
        !query_int(query, "loop", &loop, &mask, TP_LOOP) ||
        !query_int(query, "autoNext", &auto_next, &mask, TP_AUTONEXT) ||
        !query_int(query, "bold", &bold, &mask, TP_BOLD) ||
        !query_int(query, "showInfo", &values.show_info, &mask, TP_SHOW_INFO) ||
        !query_int(query, "textAlign", &values.text_align, &mask, TP_TEXT_ALIGN) ||
        !query_int(query, "marginLeft", &values.margin_left, &mask, TP_MARGIN_LEFT) ||
        !query_int(query, "marginRight", &values.margin_right, &mask, TP_MARGIN_RIGHT) ||
        !query_int(query, "textColor", &color, &mask, TP_TEXT_COLOR) ||
        !query_int(query, "mirror", &values.mirror, &mask, TP_MIRROR) ||
        !query_int(query, "sightGuide", &sight_guide, &mask, TP_SIGHT_GUIDE) ||
        !query_int(query, "centerHighlight", &center_highlight, &mask, TP_CENTER_HIGHLIGHT) ||
        ((mask & TP_LOOP) && loop!=0 && loop!=1) ||
        ((mask & TP_AUTONEXT) && auto_next!=0 && auto_next!=1) ||
        ((mask & TP_BOLD) && bold!=0 && bold!=1) ||
        ((mask & TP_SIGHT_GUIDE) && sight_guide!=0 && sight_guide!=1) ||
        ((mask & TP_CENTER_HIGHLIGHT) && center_highlight!=0 && center_highlight!=1) ||
        ((mask & TP_PLAYING) && playing != 0 && playing != 1) ||
        ((mask & TP_REWIND) && rewind != 1)) {
        return ESP_ERR_INVALID_ARG;
    }
    values.playing=playing!=0; values.speed=speed; values.font_size=font; values.rotation=rotation;
    values.brightness=brightness; values.letter_spacing=letter_spacing; values.line_spacing=line_spacing;
    values.countdown=countdown; values.loop=loop!=0; values.auto_next=auto_next!=0; values.text_color=(uint32_t)color;
    values.bold=bold!=0;
    values.sight_guide=sight_guide!=0; values.center_highlight=center_highlight!=0;
    esp_err_t err = prompter_control_apply(mask,&values);
    if(err==ESP_OK)device_settings_activity();
    return err;
}

static esp_err_t control_post(httpd_req_t *req)
{
    if(!control_session_http(req))return ESP_OK;
    if(firmware_update_in_progress()){
        httpd_resp_set_status(req,"409 Conflict");return httpd_resp_sendstr(req,"firmware_busy");
    }
    char query[512];
    if(httpd_req_get_url_query_str(req,query,sizeof(query))!=ESP_OK)
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid controls");
    esp_err_t err=apply_query(query);
      if(err==ESP_ERR_INVALID_STATE){
          httpd_resp_set_status(req,"409 Conflict");
          return httpd_resp_sendstr(req,"Control unavailable in the current playback state");
      }
      if(err!=ESP_OK)return httpd_resp_send_err(req,err==ESP_ERR_INVALID_ARG?HTTPD_400_BAD_REQUEST:HTTPD_500_INTERNAL_SERVER_ERROR,"Could not apply controls");
    return status_get(req);
}

typedef struct {
    size_t used;
    size_t body_remaining;
    bool collecting;
    bool passthrough;
    bool websocket;
    size_t websocket_payload;
    char header[2048];
} http_header_buffer_t;

static int send_response_part(httpd_handle_t server,int fd,const char *data,size_t size,int flags)
{
    http_header_buffer_t *buffer=httpd_sess_get_transport_ctx(server,fd);
    if(!buffer)return send(fd,data,size,flags);
    if(buffer->websocket){
        /* IDF sends each frame as a tiny header followed by its payload.
         * Combine our small status/control frames into a single TCP write. */
        if(buffer->websocket_payload){
            if(size!=buffer->websocket_payload||size>sizeof(buffer->header)-buffer->used){errno=EMSGSIZE;return -1;}
            memcpy(buffer->header+buffer->used,data,size);buffer->used+=size;
            size_t sent=0;
            while(sent<buffer->used){
                int n=send(fd,buffer->header+sent,buffer->used-sent,flags);
                if(n<=0)return -1;
                sent+=(size_t)n;
            }
            buffer->used=buffer->websocket_payload=0;
            return (int)size;
        }
        if(size==2||size==4){
            const uint8_t *header=(const uint8_t *)data;
            size_t payload=header[1]&0x7f;
            bool valid=!(header[0]&0x70)&&!(header[1]&0x80);
            if(size==4&&payload==126)payload=((size_t)header[2]<<8)|header[3];
            else if(size!=2||payload>=126)valid=false;
            if(valid&&payload&&payload<=sizeof(buffer->header)-size){
                memcpy(buffer->header,data,size);buffer->used=size;buffer->websocket_payload=payload;
                return (int)size;
            }
        }
        return send(fd,data,size,flags);
    }
    if(buffer->passthrough||buffer->body_remaining){
        if(buffer->body_remaining&&buffer->used){
            if(size>buffer->body_remaining||size>sizeof(buffer->header)-buffer->used){errno=EMSGSIZE;return -1;}
            memcpy(buffer->header+buffer->used,data,size);buffer->used+=size;
            buffer->body_remaining-=size;
            if(!buffer->body_remaining){
                size_t sent=0;
                while(sent<buffer->used){
                    int n=send(fd,buffer->header+sent,buffer->used-sent,flags);
                    if(n<=0)return -1;
                    sent+=(size_t)n;
                }
                buffer->used=0;
            }
            return (int)size;
        }
        int sent=send(fd,data,size,flags);
        if(sent>0&&buffer->body_remaining){
            buffer->body_remaining=(size_t)sent>=buffer->body_remaining?0:buffer->body_remaining-(size_t)sent;
        }
        return sent;
    }
    if(!buffer->collecting&&size>=9&&!memcmp(data,"HTTP/1.1 ",9))buffer->collecting=true;
    if(!buffer->collecting)return send(fd,data,size,flags);
    /* IDF emits each header field, colon and CRLF as a separate send().
     * TCP_NODELAY turns those into many radio/SDIO packets. Combine just the
     * bounded response header; keep bodies and WebSocket framing untouched. */
    if(size>sizeof(buffer->header)-buffer->used){errno=EMSGSIZE;return -1;}
    memcpy(buffer->header+buffer->used,data,size);buffer->used+=size;
    if(buffer->used>=4&&!memcmp(buffer->header+buffer->used-4,"\r\n\r\n",4)){
        if(buffer->used==sizeof(buffer->header)){errno=EMSGSIZE;return -1;}
        buffer->header[buffer->used]=0;
        const char *length=strstr(buffer->header,"\r\nContent-Length: ");
        if(length)buffer->body_remaining=strtoul(length+18,NULL,10);
        /* Once upgraded, or for chunked responses, avoid interpreting arbitrary
         * body bytes as headers. Subsequent responses remain valid passthrough. */
        buffer->websocket=!strncmp(buffer->header,"HTTP/1.1 101 ",13);
        buffer->passthrough=strstr(buffer->header,"\r\nTransfer-Encoding:")!=NULL ||buffer->websocket;
        buffer->collecting=false;
        /* Small JSON responses should arrive in one packet, rather than wait
         * for the receiver's delayed ACK between headers and body. */
        if(!buffer->passthrough&&buffer->body_remaining&&
           buffer->body_remaining<=sizeof(buffer->header)-buffer->used)return (int)size;
        size_t sent=0;
        while(sent<buffer->used){
            int n=send(fd,buffer->header+sent,buffer->used-sent,flags);
            if(n<=0)return -1;
            sent+=(size_t)n;
        }
        buffer->used=0;buffer->collecting=false;
    }
    return (int)size;
}

static esp_err_t connection_open(httpd_handle_t server,int fd)
{
    http_header_buffer_t *buffer=calloc(1,sizeof(*buffer));
    if(!buffer)return ESP_ERR_NO_MEM;
    httpd_sess_set_transport_ctx(server,fd,buffer,free);
    if(httpd_sess_set_send_override(server,fd,send_response_part)!=ESP_OK)return ESP_FAIL;
    int enabled=1;
    return setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&enabled,sizeof(enabled))==0?ESP_OK:ESP_FAIL;
}

static bool s_push_force;
typedef struct {char client[33];uint32_t token;} socket_reader_t;
static uint32_t s_socket_token;
static void release_socket_reader(void *context)
{
    socket_reader_t *reader=context;
    if(reader){device_settings_presence_socket(reader->client,reader->token,false);free(reader);}
}
static esp_err_t socket_handshake(httpd_req_t *req)
{
    if(!control_session_http(req))return ESP_FAIL;
    /* IDF 6.1 completes an upgrade without calling the URI handler. Attach
     * the reader while the handshake URL is still available, so subsequent
     * WebSocket frames have the client identity in their session context. */
    char query[64],client[33]={0};
    if(httpd_req_get_url_query_str(req,query,sizeof(query))==ESP_OK&&
       httpd_query_key_value(query,"client",client,sizeof(client))==ESP_OK){
        size_t length=strlen(client);
        if(length<8||length>32)return ESP_ERR_INVALID_ARG;
        for(size_t i=0;i<length;++i)if(!((client[i]>='0'&&client[i]<='9')||(client[i]>='a'&&client[i]<='f')))return ESP_ERR_INVALID_ARG;
        socket_reader_t *reader=calloc(1,sizeof(*reader));
        if(!reader)return ESP_ERR_NO_MEM;
        snprintf(reader->client,sizeof(reader->client),"%s",client);
        reader->token=++s_socket_token;if(!reader->token)reader->token=++s_socket_token;
        req->sess_ctx=reader;req->free_ctx=release_socket_reader;
        device_settings_presence_socket(client,reader->token,true);
        control_session_route_presence(req,client);
        wifi_settings_confirm_lan(req,client);
    }
    /* A dead live-view client must not hold the shared HTTP task for the
     * default five-second asset transfer timeout. */
    int enabled=1;
    setsockopt(httpd_req_to_sockfd(req),IPPROTO_TCP,TCP_NODELAY,&enabled,sizeof(enabled));
    const struct timeval timeout={.tv_sec=0,.tv_usec=250000};
    setsockopt(httpd_req_to_sockfd(req),SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    s_push_force=true;
    return ESP_OK;
}
static esp_err_t control_socket(httpd_req_t *req)
{
    if(!control_session_http(req))return ESP_OK;
    httpd_ws_frame_t frame={0};
    ESP_RETURN_ON_ERROR(httpd_ws_recv_frame(req,&frame,0),TAG,"WS header");
    if(frame.type!=HTTPD_WS_TYPE_TEXT||frame.len>640)return ESP_FAIL;
    char payload[641];frame.payload=(uint8_t*)payload;
    ESP_RETURN_ON_ERROR(httpd_ws_recv_frame(req,&frame,sizeof(payload)-1),TAG,"WS payload");
    payload[frame.len]=0;
    cJSON *message=cJSON_Parse(payload);
    if(cJSON_IsTrue(cJSON_GetObjectItem(message,"heartbeat"))){
        const socket_reader_t *reader=req->sess_ctx;
        const cJSON *reading=cJSON_GetObjectItem(message,"reading");
        if(reader&&cJSON_IsBool(reading)){
            device_settings_presence(reader->client,cJSON_IsTrue(reading),false);
            control_session_route_presence(req,reader->client);
        }
        cJSON_Delete(message);char body[1280];status_body(body,sizeof(body));
        httpd_ws_frame_t out={.type=HTTPD_WS_TYPE_TEXT,.payload=(uint8_t*)body,.len=strlen(body)};
        return httpd_ws_send_frame(req,&out);
    }
    const cJSON *id=cJSON_GetObjectItem(message,"id"),*query=cJSON_GetObjectItem(message,"query");
    if(!cJSON_IsNumber(id)||!cJSON_IsString(query)||strlen(query->valuestring)>511){cJSON_Delete(message);return ESP_FAIL;}
    esp_err_t err=firmware_update_in_progress()?ESP_ERR_INVALID_STATE:apply_query(query->valuestring);
    char body[1280],reply[1408];status_body(body,sizeof(body));
    int n=snprintf(reply,sizeof(reply),"{\"id\":%d,\"error\":%d,\"state\":%s}",id->valueint,(int)err,body);
    cJSON_Delete(message);
    if(n<=0||n>=(int)sizeof(reply))return ESP_FAIL;
    httpd_ws_frame_t out={.type=HTTPD_WS_TYPE_TEXT,.payload=(uint8_t*)reply,.len=(size_t)n};
    return httpd_ws_send_frame(req,&out);
}

static atomic_bool s_push_pending;
static void push_status(void *arg)
{
    (void)arg;
    static uint32_t last_revision,last_preview_revision,last_settings_revision,last_library_revision;
    static bool last_counting;
    static char last_lifecycle[32];
    static int64_t last_sent;
    prompter_status_t status;
    prompter_snapshot(&status,NULL);
    const char *lifecycle=teleprompter_lifecycle();
    const int64_t now=esp_timer_get_time();
    const bool counting=status.countdown_remaining_ms>0;
    uint32_t settings_revision=device_settings_revision(),library_revision=script_library_revision();
    const bool changed=status.revision!=last_revision || status.preview_revision!=last_preview_revision ||
        settings_revision!=last_settings_revision || library_revision!=last_library_revision ||
        counting!=last_counting || strcmp(lifecycle,last_lifecycle);
    /* Controls and lifecycle changes propagate at the next 100-ms tick.
     * Position samples need only 2 Hz: clients interpolate the scroll locally.
     * An idle heartbeat stays below the clients' 1.5-second fallback threshold. */
    const int64_t interval=status.playing||counting?500000:1000000;
    if(!s_push_force&&!changed&&now-last_sent<interval)goto done;
    s_push_force=false;
    last_revision=status.revision;last_preview_revision=status.preview_revision;
    last_settings_revision=settings_revision;last_library_revision=library_revision;
    last_counting=counting;snprintf(last_lifecycle,sizeof(last_lifecycle),"%s",lifecycle);
    last_sent=now;
    size_t count=12;int clients[12];
    if(httpd_get_client_list(s_server,&count,clients)==ESP_OK){
        char body[1280];status_body(body,sizeof(body));
        httpd_ws_frame_t frame={.type=HTTPD_WS_TYPE_TEXT,.payload=(uint8_t*)body,.len=strlen(body)};
        for(size_t i=0;i<count;++i)if(httpd_ws_get_fd_info(s_server,clients[i])==HTTPD_WS_CLIENT_WEBSOCKET){
            const socket_reader_t *reader=httpd_sess_get_ctx(s_server,clients[i]);
            if(!control_session_socket(s_server,clients[i],false)){httpd_sess_trigger_close(s_server,clients[i]);continue;}
            if(reader)device_settings_client_seen(reader->client);
            if(httpd_ws_send_frame_async(s_server,clients[i],&frame)!=ESP_OK)httpd_sess_trigger_close(s_server,clients[i]);
            else httpd_sess_update_lru_counter(s_server,clients[i]);
        }
    }
done:
    atomic_store(&s_push_pending,false);
}
static void schedule_status(void *arg)
{
    (void)arg;
    if(!atomic_exchange(&s_push_pending,true)&&httpd_queue_work(s_server,push_status,NULL)!=ESP_OK)
        atomic_store(&s_push_pending,false);
}

static esp_err_t device_settings_get(httpd_req_t *req)
{
    if(!control_session_http(req))return ESP_OK;
    char body[2048];
    if (device_settings_json(body, sizeof(body)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t device_settings_post(httpd_req_t *req)
{
    if(!control_session_http(req))return ESP_OK;
    char body[513];
    if (req->content_len <= 0 || req->content_len >= sizeof(body))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid settings length");
    int received=0;
    while (received < req->content_len) {
        int n=httpd_req_recv(req,body+received,req->content_len-received);
        if(n<=0)return ESP_FAIL;
        received+=n;
    }
    if (memchr(body, 0, received))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid settings");
    body[received]=0;
    esp_err_t err=device_settings_update(body);
    if(err==ESP_ERR_INVALID_STATE){
        httpd_resp_set_status(req,"409 Conflict");
        return httpd_resp_sendstr(req,"firmware_busy");
    }
    if(err==ESP_ERR_INVALID_ARG)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid device settings");
    if(err!=ESP_OK)return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save settings");
    return device_settings_get(req);
}

static esp_err_t device_action_post(httpd_req_t *req)
{
    if(!control_session_http(req))return ESP_OK;
    if(firmware_update_in_progress()){
        httpd_resp_set_status(req,"409 Conflict");
        return httpd_resp_sendstr(req,"firmware_busy");
    }
    char query[64], action[20];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "action", action, sizeof(action)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing action");
    tp_device_action_t selected = !strcmp(action, "restart") ? TP_DEVICE_RESTART :
        !strcmp(action, "standby") ? TP_DEVICE_STANDBY :
        !strcmp(action, "shutdown") ? TP_DEVICE_SHUTDOWN :
        !strcmp(action, "wake") ? TP_DEVICE_WAKE : TP_DEVICE_NONE;
    if (selected == TP_DEVICE_NONE)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid action");
    if (teleprompter_device_action(selected) != ESP_OK) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "Device starting or busy");
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

esp_err_t network_control_start(void)
{
    ESP_LOGI(TAG, "BOARD_BUSES Wi-Fi=SDMMC slot 1; TF card=SPI2; LCD=MIPI DSI");
    esp_err_t err = nvs_flash_init();
    ESP_RETURN_ON_ERROR(err, TAG, "NVS init");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    ESP_RETURN_ON_ERROR(esp_hosted_init(), TAG, "Hosted init");
    ESP_RETURN_ON_ERROR(esp_hosted_connect_to_slave(), TAG, "C6 connection");
    char target[24] = {0};
    uint32_t chip = 0;
    err = esp_hosted_get_cp_info(&chip, target, sizeof(target));
    ESP_LOGI(TAG, "C6 info: %s chip=%lu result=%s", target, (unsigned long)chip, esp_err_to_name(err));
    if (!esp_netif_create_default_wifi_ap()) return ESP_ERR_NO_MEM;
    if (!esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "remote Wi-Fi init");
    uint8_t device_mac[6];
    ESP_RETURN_ON_ERROR(esp_wifi_get_mac(WIFI_IF_AP,device_mac), TAG, "device MAC");
    device_settings_radio_identity(device_mac);
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Wi-Fi storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA), TAG, "AP + station mode");
    wifi_config_t wifi = {.ap = {
        .ssid = "Teleprompter-P4", .ssid_len = sizeof("Teleprompter-P4") - 1,
        .channel = 6, .max_connection = 10,
        .authmode = WIFI_AUTH_WPA2_PSK, .beacon_interval = 100,
        .dtim_period = 2, .pmf_cfg = {.capable = true},
    }};
    char ap_name[33];
    device_settings_boot_names(ap_name,sizeof(ap_name),NULL,0);
    memset(wifi.ap.ssid,0,sizeof(wifi.ap.ssid));
    wifi.ap.ssid_len=strlen(ap_name);
    memcpy(wifi.ap.ssid,ap_name,wifi.ap.ssid_len);
    char ap_password[64];device_settings_boot_password(ap_password,sizeof(ap_password));
    wifi.ap.authmode=ap_password[0]?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;
    memcpy(wifi.ap.password,ap_password,strlen(ap_password));
    esp_err_t ap_error=esp_wifi_set_config(WIFI_IF_AP, &wifi);
    memset(ap_password,0,sizeof(ap_password));
    memset(wifi.ap.password,0,sizeof(wifi.ap.password));
    ESP_RETURN_ON_ERROR(ap_error, TAG, "AP config");
    ESP_RETURN_ON_ERROR(wifi_settings_init(), TAG, "saved Wi-Fi settings");
    ESP_LOGI(TAG, "AP configured; starting C6 Wi-Fi");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "AP start");
    esp_err_t power_save=esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(TAG,"Low-latency Wi-Fi power save disable: %s",esp_err_to_name(power_save));
    wifi_settings_start();
    ESP_LOGI(TAG, "C6 Wi-Fi started; starting HTTP server");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 24;
    config.max_open_sockets = 12;
    config.backlog_conn = 8;
    config.lru_purge_enable = true;
    config.keep_alive_enable = true;
    config.keep_alive_idle = 15;
    config.keep_alive_interval = 5;
    config.keep_alive_count = 3;
    config.open_fn = connection_open;
    config.stack_size = 12288;
    ESP_RETURN_ON_ERROR(web_assets_init(),TAG,"Asset slots");
    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "HTTP start");
    const httpd_uri_t uris[] = {
        {.uri="/", .method=HTTP_GET, .handler=asset_get},
        {.uri="/reader.woff2", .method=HTTP_GET, .handler=asset_get},
        {.uri="/api/status", .method=HTTP_GET, .handler=status_get},
        {.uri="/api/script", .method=HTTP_GET, .handler=script_get},
        {.uri="/api/script", .method=HTTP_PUT, .handler=script_put},
        {.uri="/api/control", .method=HTTP_POST, .handler=control_post},
        {.uri="/api/live", .method=HTTP_GET, .handler=control_socket, .is_websocket=true, .ws_pre_handshake_cb=socket_handshake},
        {.uri="/api/display/retry", .method=HTTP_POST, .handler=display_retry_post},
        {.uri="/api/device", .method=HTTP_POST, .handler=device_action_post},
        {.uri="/api/settings", .method=HTTP_GET, .handler=device_settings_get},
        {.uri="/api/settings", .method=HTTP_POST, .handler=device_settings_post},
        {.uri="/api/library", .method=HTTP_POST, .handler=library_post},
    };
    for (size_t i = 0; i < sizeof(uris)/sizeof(uris[0]); ++i) {
        err = httpd_register_uri_handler(s_server, &uris[i]);
        if (err != ESP_OK) { httpd_stop(s_server); s_server = NULL; return err; }
    }
    ESP_RETURN_ON_ERROR(wifi_settings_register(s_server), TAG, "Wi-Fi settings routes");
    ESP_RETURN_ON_ERROR(captive_portal_start(s_server), TAG, "Legacy controller routes");
    esp_timer_handle_t push_timer;
    const esp_timer_create_args_t push_args={.callback=schedule_status,.name="control_push"};
    ESP_RETURN_ON_ERROR(esp_timer_create(&push_args,&push_timer),TAG,"control push timer");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(push_timer,100000),TAG,"control push start");
    char ap_address[16];wifi_settings_hotspot_address(ap_address,sizeof(ap_address));
    ESP_LOGI(TAG, "WEB_READY SSID=%s address=http://%s",ap_name,ap_address);
    return ESP_OK;
}
