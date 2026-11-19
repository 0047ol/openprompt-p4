#pragma once
#include <stdbool.h>

/* Inclusive physical panel coordinates; one accumulator per scan buffer. */
typedef struct { int left,top,right,bottom; bool valid; } tp_display_damage_t;
static inline void tp_display_damage_add(tp_display_damage_t *area,int left,int top,int right,int bottom)
{
    if(left>right||top>bottom)return;
    if(!area->valid){*area=(tp_display_damage_t){left,top,right,bottom,true};return;}
    if(left<area->left)area->left=left;
    if(top<area->top)area->top=top;
    if(right>area->right)area->right=right;
    if(bottom>area->bottom)area->bottom=bottom;
}

/* Keep distant labels separate rather than copying the gap between them.
 * Overflow conservatively coalesces; no invalidated pixels are dropped. */
#define TP_DISPLAY_DAMAGE_LIMIT 16
typedef struct { tp_display_damage_t areas[TP_DISPLAY_DAMAGE_LIMIT]; unsigned count; } tp_display_damage_set_t;
static inline void tp_display_damage_set_add(tp_display_damage_set_t *set,int left,int top,int right,int bottom)
{
    if(left>right||top>bottom)return;
    tp_display_damage_t next={left,top,right,bottom,true};
    for(unsigned i=0;i<set->count;){
        const tp_display_damage_t *area=&set->areas[i];
        bool overlap=next.left<=area->right&&next.right>=area->left&&next.top<=area->bottom&&next.bottom>=area->top;
        bool vertical=next.left==area->left&&next.right==area->right&&next.top<=area->bottom+1&&next.bottom>=area->top-1;
        bool horizontal=next.top==area->top&&next.bottom==area->bottom&&next.left<=area->right+1&&next.right>=area->left-1;
        if(overlap||vertical||horizontal){
            tp_display_damage_add(&next,area->left,area->top,area->right,area->bottom);
            set->areas[i]=set->areas[--set->count];i=0;
        }else ++i;
    }
    if(set->count==TP_DISPLAY_DAMAGE_LIMIT){
        for(unsigned i=0;i<set->count;++i){const tp_display_damage_t *area=&set->areas[i];tp_display_damage_add(&next,area->left,area->top,area->right,area->bottom);}
        set->count=0;
    }
    set->areas[set->count++]=next;
}
