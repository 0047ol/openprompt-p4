#include "control_session.h"
#include "device_settings.h"
#include "wifi_settings.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "lwip/sockets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct peer_state {struct peer_state *next;char name[64];int64_t seen;bool console,hotspot;} peer_state_t;
static peer_state_t *s_peers;
static portMUX_TYPE s_lock=portMUX_INITIALIZER_UNLOCKED;
static tp_link_mode_t s_mode=TP_LINK_HOTSPOT;
#define SESSION_LEASE_US INT64_C(15000000)
static bool bluetooth(const char *peer){return !strncmp(peer,"ble",3);}
static bool active(const peer_state_t *p,int64_t now){return p->seen&&now-p->seen<SESSION_LEASE_US;}
static peer_state_t *find(const char *peer){for(peer_state_t *p=s_peers;p;p=p->next)if(!strcmp(p->name,peer))return p;return NULL;}
tp_link_mode_t control_session_mode(void){portENTER_CRITICAL(&s_lock);tp_link_mode_t m=s_mode;portEXIT_CRITICAL(&s_lock);return m;}
const char *control_session_mode_name(void){return control_session_mode()==TP_LINK_LAN?"lan":"hotspot";}
void control_session_set_mode(tp_link_mode_t mode){portENTER_CRITICAL(&s_lock);s_mode=mode;portEXIT_CRITICAL(&s_lock);}
bool control_session_claim(const char *peer)
{
    if(!peer||!*peer||strlen(peer)>=64)return false;
    portENTER_CRITICAL(&s_lock);peer_state_t *p=find(peer);
    if(p)p->seen=esp_timer_get_time();
    portEXIT_CRITICAL(&s_lock);
    if(p)return true;
    peer_state_t *allocated=calloc(1,sizeof(*allocated));if(!allocated)return false;
    snprintf(allocated->name,sizeof(allocated->name),"%s",peer);
    portENTER_CRITICAL(&s_lock);p=find(peer);
    if(!p){
        for(p=s_peers;p;p=p->next)if(!active(p,esp_timer_get_time()))break;
        if(p){snprintf(p->name,sizeof(p->name),"%s",peer);p->console=false;p->hotspot=false;}
        else{p=allocated;allocated=NULL;p->next=s_peers;s_peers=p;}
    }
    p->seen=esp_timer_get_time();portEXIT_CRITICAL(&s_lock);free(allocated);return true;
}
void control_session_release(const char *peer)
{
    if(!peer)return;
    portENTER_CRITICAL(&s_lock);peer_state_t *p=find(peer);if(p){p->seen=0;p->console=false;}portEXIT_CRITICAL(&s_lock);
    device_settings_presence_disconnect(peer,bluetooth(peer)?3:0);
}
static bool socket_peer(int fd,char *peer,size_t size)
{
    struct sockaddr_storage addr={0};socklen_t length=sizeof(addr);
    if(getpeername(fd,(struct sockaddr *)&addr,&length))return false;
    if(addr.ss_family==AF_INET)return inet_ntop(AF_INET,&((struct sockaddr_in *)&addr)->sin_addr,peer,size)!=NULL;
#if CONFIG_LWIP_IPV6
    if(addr.ss_family==AF_INET6){struct in6_addr *ip=&((struct sockaddr_in6 *)&addr)->sin6_addr;
        if(IN6_IS_ADDR_V4MAPPED(ip))return inet_ntop(AF_INET,&ip->s6_addr[12],peer,size)!=NULL;
        return inet_ntop(AF_INET6,ip,peer,size)!=NULL;}
#endif
    return false;
}
bool control_session_socket(httpd_handle_t server,int fd,bool renew)
{
    (void)server;char peer[64];if(!socket_peer(fd,peer,sizeof(peer)))return false;
    if(renew)return control_session_claim(peer);
    portENTER_CRITICAL(&s_lock);peer_state_t *p=find(peer);bool valid=p&&active(p,esp_timer_get_time());portEXIT_CRITICAL(&s_lock);return valid;
}
bool control_session_http(httpd_req_t *req)
{
    if(control_session_socket(req->handle,httpd_req_to_sockfd(req),true))return true;
    httpd_resp_set_status(req,"503 Service Unavailable");httpd_resp_set_type(req,"application/json");
    httpd_resp_sendstr(req,"{\"ok\":false,\"error\":\"out_of_memory\"}");return false;
}
static bool socket_hotspot(int fd)
{
    char ap[16];wifi_settings_hotspot_address(ap,sizeof(ap));
    in_addr_t address=inet_addr(ap);
    struct sockaddr_storage local={0};socklen_t length=sizeof(local);
    if(getsockname(fd,(struct sockaddr *)&local,&length))return false;
    if(local.ss_family==AF_INET)return ((struct sockaddr_in *)&local)->sin_addr.s_addr==address;
#if CONFIG_LWIP_IPV6
    if(local.ss_family==AF_INET6){struct in6_addr *ip=&((struct sockaddr_in6 *)&local)->sin6_addr;
        if(IN6_IS_ADDR_V4MAPPED(ip)){struct in_addr v4;memcpy(&v4,&ip->s6_addr[12],sizeof(v4));return v4.s_addr==address;}}
#endif
    return false;
}
bool control_session_http_hotspot(httpd_req_t *req){return socket_hotspot(httpd_req_to_sockfd(req));}
static void console_seen(const char *peer,bool hotspot)
{
    if(!control_session_claim(peer))return;
    portENTER_CRITICAL(&s_lock);peer_state_t *p=find(peer);p->console=true;p->hotspot=hotspot;portEXIT_CRITICAL(&s_lock);
}
void control_session_console_http(httpd_req_t *req){char peer[64];int fd=httpd_req_to_sockfd(req);if(socket_peer(fd,peer,sizeof(peer)))console_seen(peer,socket_hotspot(fd));}
void control_session_route_presence(httpd_req_t *req,const char *client)
{
    char peer[64];int fd=httpd_req_to_sockfd(req);
    if(socket_peer(fd,peer,sizeof(peer)))device_settings_presence_route(client,peer,socket_hotspot(fd),-1);
}
bool control_session_ble(void){return control_session_claim("ble");}
void control_session_console_ble(void){console_seen("ble",false);}
bool control_session_ble_peer(unsigned handle,bool ready){char peer[32];snprintf(peer,sizeof(peer),"ble:%u",handle);if(ready)console_seen(peer,false);return control_session_claim(peer);}
void control_session_ble_release(unsigned handle){char peer[32];snprintf(peer,sizeof(peer),"ble:%u",handle);control_session_release(peer);}
static bool connected_kind(int kind)
{
    bool result=false;int64_t now=esp_timer_get_time();portENTER_CRITICAL(&s_lock);
    for(peer_state_t *p=s_peers;p;p=p->next)if(p->console&&active(p,now)&&(kind==0||(kind==1&&!bluetooth(p->name))||(kind==2&&bluetooth(p->name))||(kind==3&&!bluetooth(p->name)&&p->hotspot))){result=true;break;}
    portEXIT_CRITICAL(&s_lock);return result;
}
bool control_session_connected(void){return connected_kind(0);}
bool control_session_network_connected(void){return connected_kind(1);}
bool control_session_bluetooth_connected(void){return connected_kind(2);}
bool control_session_hotspot_controller(void){return connected_kind(3);}
bool control_session_network_controller(httpd_req_t *req){return control_session_socket(req->handle,httpd_req_to_sockfd(req),false);}
void control_session_network_failed(bool failed){if(failed)control_session_network_disconnected(false);}
void control_session_network_disconnected(bool hotspot)
{
    portENTER_CRITICAL(&s_lock);for(peer_state_t *p=s_peers;p;p=p->next)if(!bluetooth(p->name)&&p->hotspot==hotspot){p->seen=0;p->console=false;}portEXIT_CRITICAL(&s_lock);
    device_settings_presence_disconnect(NULL,hotspot?1:2);
}
bool control_session_portal_open(void){return !control_session_connected();}
void control_session_lan_ready(void){}
bool control_session_network_available(httpd_req_t *req){(void)req;return true;}
bool control_session_portal_available(httpd_req_t *req,bool reserve){(void)req;(void)reserve;return false;}
bool control_session_portal_http(httpd_req_t *req){httpd_resp_set_status(req,"410 Gone");httpd_resp_sendstr(req,"{\"ok\":false,\"error\":\"portal_removed\"}");return false;}
bool control_session_portal_language(httpd_req_t *req){(void)req;return true;}
