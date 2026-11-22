/* Whole-page reading: blank manuscript rows separate pages without editing text. */
#pragma once
#include <stdint.h>

typedef struct { uint16_t first, count; } tp_page_span_t;

static inline unsigned tp_page_build(const char *text, const uint32_t *offsets,
                                    unsigned rows, unsigned capacity, unsigned paragraphs, tp_page_span_t *pages)
{
    unsigned total=0;
    if(!capacity)capacity=1;
    for(unsigned row=0;row<rows;++row){
        unsigned ink=0;
        for(uint32_t at=offsets[row];at<offsets[row+1];++at)
            if((unsigned char)text[at]>' '){ink=1;break;}
        if(paragraphs&&!ink)continue;
        if(!total||pages[total-1].count>=capacity||
           pages[total-1].first+pages[total-1].count!=row)
            pages[total++]=(tp_page_span_t){row,0};
        ++pages[total-1].count;
    }
    /* An all-blank document still has one usable page. */
    if(!total)pages[total++]=(tp_page_span_t){0,1};
    return total;
}
