#pragma once
#include <stdint.h>

/* Cache-line coverage of CPU-written RGB565 chrome. The physical row stride
 * is a multiple of 64 bytes; tracking lines avoids synchronizing the entire
 * 2 MB image for narrow rotated header/footer updates. */
#define TP_DISPLAY_CACHE_WIDTH 800
#define TP_DISPLAY_CACHE_HEIGHT 1280
#define TP_DISPLAY_CACHE_LINE_BYTES 64
#define TP_DISPLAY_CACHE_LINE_COUNT (TP_DISPLAY_CACHE_WIDTH*TP_DISPLAY_CACHE_HEIGHT*2/TP_DISPLAY_CACHE_LINE_BYTES)
typedef struct { uint32_t words[(TP_DISPLAY_CACHE_LINE_COUNT+31)/32]; } tp_display_cache_t;

static inline void tp_display_cache_mark(tp_display_cache_t *set,int left,int top,int right,int bottom)
{
    if(left<0)left=0;
    if(top<0)top=0;
    if(right>=TP_DISPLAY_CACHE_WIDTH)right=TP_DISPLAY_CACHE_WIDTH-1;
    if(bottom>=TP_DISPLAY_CACHE_HEIGHT)bottom=TP_DISPLAY_CACHE_HEIGHT-1;
    if(left>right||top>bottom)return;
    for(int y=top;y<=bottom;++y){
        unsigned first=(y*TP_DISPLAY_CACHE_WIDTH+left)*2/TP_DISPLAY_CACHE_LINE_BYTES;
        unsigned last=(y*TP_DISPLAY_CACHE_WIDTH+right)*2/TP_DISPLAY_CACHE_LINE_BYTES;
        for(unsigned line=first;line<=last;++line)set->words[line/32]|=UINT32_C(1)<<(line%32);
    }
}

/* Return one contiguous aligned range, or LINE_COUNT when there is no more.
 * Callers advance to first+count. Do not clear coverage until all writes have
 * reached memory, before any DMA reads the chrome buffer. */
static inline unsigned tp_display_cache_next(const tp_display_cache_t *set,unsigned start,unsigned *count)
{
    unsigned first=TP_DISPLAY_CACHE_LINE_COUNT;
    while(start<TP_DISPLAY_CACHE_LINE_COUNT){
        uint32_t word=set->words[start/32]&(UINT32_MAX<<(start%32));
        if(word){first=(start/32)*32+(unsigned)__builtin_ctz(word);break;}
        start=(start/32+1)*32;
    }
    unsigned end=first;
    while(end<TP_DISPLAY_CACHE_LINE_COUNT&&(set->words[end/32]&(UINT32_C(1)<<(end%32))))++end;
    *count=end-first;
    return first;
}
