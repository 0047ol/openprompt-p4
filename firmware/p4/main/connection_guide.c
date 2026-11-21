#include "connection_guide.h"
#include <stdio.h>
#include <string.h>

bool tp_guide_format_body(char *out, size_t capacity, const char *format,
                          const char *hotspot, const char *ssid, const char *bluetooth, const char *url)
{
    if(!out||!capacity||!format)return false;
    const char *keys[]={"{hotspot}","{ssid}","{bluetooth}","{url}"};
    const char *values[]={hotspot,ssid,bluetooth,url};
    size_t used=0;
    while(*format){
        const char *value=format;size_t length=1,consumed=1;
        for(unsigned i=0;i<4;i++){
            size_t key_size=strlen(keys[i]);
            if(!strncmp(format,keys[i],key_size)){
                value=values[i]?values[i]:"";length=strlen(value);consumed=key_size;break;
            }
        }
        if(length>=capacity-used){out[0]=0;return false;}
        memcpy(out+used,value,length);used+=length;format+=consumed;
    }
    out[used]=0;return true;
}

void tp_guide_peer_update(tp_guide_peers_t *peers, const unsigned char mac[6], bool joined)
{
    unsigned empty=TP_GUIDE_PEER_CAPACITY;
    for(unsigned i=0;i<TP_GUIDE_PEER_CAPACITY;i++){
        if(peers->used[i]&&!memcmp(peers->mac[i],mac,6)){peers->used[i]=joined;return;}
        if(!peers->used[i])empty=i;
    }
    if(joined&&empty<TP_GUIDE_PEER_CAPACITY){memcpy(peers->mac[empty],mac,6);peers->used[empty]=true;}
}
unsigned tp_guide_peer_count(const tp_guide_peers_t *peers)
{
    unsigned count=0;for(unsigned i=0;i<TP_GUIDE_PEER_CAPACITY;i++)count+=peers->used[i];return count;
}

tp_guide_stage_t tp_guide_stage(tp_guide_tracker_t *tracker, const tp_guide_input_t *input, uint32_t now_ms)
{
    bool repairing=input->portal_open&&input->wifi_enabled&&!input->wifi_connected;
    /* Failed Wi-Fi returns to the normal hotspot flow. Once a phone joins,
     * advance to its controller URL instead of trapping it on the join QR. */
    tp_guide_stage_t hotspot_stage=input->hotspot_clients?TP_GUIDE_LAN:TP_GUIDE_JOIN;
    tp_guide_stage_t next=repairing?(input->wifi_failed?hotspot_stage:TP_GUIDE_WIFI_CONNECTING):
        input->controller?TP_GUIDE_READY:
        !input->portal_open?TP_GUIDE_RECONNECT:
        input->bluetooth_selected?TP_GUIDE_BLUETOOTH:
        input->wifi_connected?TP_GUIDE_LAN:
        input->wifi_enabled?(input->wifi_failed?hotspot_stage:TP_GUIDE_WIFI_CONNECTING):
        hotspot_stage;
    /* A short Wi-Fi reassociation must not replace an address QR mid-scan. */
    if(tracker->initialized&&tracker->stage==TP_GUIDE_LAN&&next==TP_GUIDE_JOIN&&!input->wifi_failed){
        if(tracker->pending!=next){tracker->pending=next;tracker->pending_at=now_ms;}
        if((uint32_t)(now_ms-tracker->pending_at)<3000)return tracker->stage;
    }else{tracker->pending=next;tracker->pending_at=now_ms;}
    tracker->initialized=true;tracker->stage=next;return next;
}

/* Online and temporarily disconnected controllers share one standby view. */
bool tp_guide_animate_step(bool presented, bool initialized, tp_guide_stage_t before, tp_guide_stage_t after)
{
    if(!presented||!initialized||before==after)return false;
    bool before_standby=before==TP_GUIDE_READY||before==TP_GUIDE_RECONNECT;
    bool after_standby=after==TP_GUIDE_READY||after==TP_GUIDE_RECONNECT;
    return !(before_standby&&after_standby);
}

tp_guide_stage_t tp_guide_content_stage(tp_guide_stage_t stage, const tp_guide_input_t *input)
{
    if(stage!=TP_GUIDE_READY&&stage!=TP_GUIDE_RECONNECT)return stage;
    return input->bluetooth_selected||(!input->wifi_connected&&!input->hotspot)
        ?TP_GUIDE_BLUETOOTH:TP_GUIDE_LAN;
}

bool tp_guide_wifi_payload(char *out, size_t capacity, const char *name, const char *password)
{
    if(!out||!capacity||!name||!password)return false;
    int n=snprintf(out,capacity,"WIFI:T:%s;S:",password[0]?"WPA":"nopass");
    if(n<0||(size_t)n>=capacity){out[0]=0;return false;}
    size_t used=(size_t)n;
    const char *fields[]={name,password};
    for(unsigned i=0;i<2;i++){
        for(const char *p=fields[i];*p;p++){
            size_t count=strchr("\\;,:\"",*p)?2:1;
            if(used+count>=capacity){out[0]=0;return false;}
            if(count==2)out[used++]='\\';
            out[used++]=*p;
        }
        const char *end=i==0?";P:":";;";
        size_t length=strlen(end);
        if(used+length>=capacity){out[0]=0;return false;}
        memcpy(out+used,end,length);used+=length;
    }
    out[used]=0;return true;
}
