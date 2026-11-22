#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Small, bounded HID parser: Consumer Control arrays and variable volume keys.
 * Unknown layouts are rejected rather than guessed from arbitrary bytes. */
typedef struct { uint16_t offset,usage; uint8_t bits,count,id; bool variable; } tp_hid_field;
typedef struct { tp_hid_field fields[12]; unsigned count; } tp_hid_map;
static inline bool tp_hid_parse(tp_hid_map *map,const uint8_t *data,size_t size)
{
    *map=(tp_hid_map){0};uint32_t page=0,bits=0,count=0,id=0,usage=0,min=0;unsigned offsets[256]={0};
    for(size_t p=0;p<size;){
        uint8_t tag=data[p++];if(tag==0xfe)return false;
        unsigned n=(tag&3)==3?4:tag&3;if(p+n>size)return false;
        uint32_t v=0;for(unsigned i=0;i<n;i++)v|=(uint32_t)data[p++]<<(i*8);
        switch(tag&0xfc){
        case 0x04:page=v;break;case 0x74:bits=v;break;case 0x94:count=v;break;
        case 0x84:if(!v||v>255)return false;id=v;break;
        case 0x08:if(!usage)usage=v;break;case 0x18:min=v;break;
        case 0x80:
            if(bits>32||count>128||offsets[id]+bits*count>1024)return false;
            if(page==0x0c&&!(v&1)&&bits&&count){
                if(map->count>=12)return false;
                map->fields[map->count++]=(tp_hid_field){offsets[id],usage?usage:min,bits,count,id,!!(v&2)};
            }
            offsets[id]+=bits*count;usage=min=0;break;
        /* Global PUSH/POP are not handled by this compact profile. */
        case 0xa4:case 0xb4:return false;
        default:if((tag&0x0c)==0)usage=min=0;break;
        }
    }
    return map->count>0;
}
static inline bool tp_hid_report(const tp_hid_map *map,uint8_t id,const uint8_t *data,size_t size)
{
    for(unsigned i=0;i<map->count;i++){
        const tp_hid_field *f=&map->fields[i];if(f->id!=id)continue;
        if(f->offset+(unsigned)f->bits*f->count>size*8)continue;
        for(unsigned j=0;j<f->count;j++){
            uint32_t value=0;unsigned at=f->offset+j*f->bits;
            for(unsigned k=0;k<f->bits;k++)value|=((data[(at+k)/8]>>((at+k)%8))&1U)<<k;
            unsigned usage=f->variable?f->usage+j:value;
            if(value&&(usage==0xe9||usage==0xea))return true;
        }
    }
    return false;
}
/* One action per click group; releases/duplicates do not trigger actions.
 * The single action waits for the double-click window by design. */
typedef struct { bool down,pending; uint64_t first,last; } tp_remote_clicks;
static inline unsigned tp_remote_input(tp_remote_clicks *s,bool down,uint64_t now)
{
    if(!down){s->down=false;return 0;}if(s->down)return 0;s->down=true;
    if(s->last&&now-s->last<30000)return 0;
    s->last=now;
    if(s->pending&&now-s->first<=350000){s->pending=false;return 2;}
    s->pending=true;s->first=now;return 0;
}
static inline unsigned tp_remote_tick(tp_remote_clicks *s,uint64_t now)
{if(s->pending&&now-s->first>350000){s->pending=false;return 1;}return 0;}
