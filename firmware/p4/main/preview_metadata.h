#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The immutable layout can contain thousands of rows. Updating four numeric
 * fields must not materialize that JSON tree in scarce internal/DMA memory.
 * Preserve every other byte, including manuscript text and glyph advances. */
static inline bool tp_preview_metadata_update(const char *source,char *output,size_t capacity,
                                              uint32_t revision,int mirror,int text_top,int end_scroll)
{
    const char *keys[]={"\"revision\":","\"mirror\":","\"textTop\":","\"endScroll\":"};
    struct {const char *start,*end;char value[16];} fields[4];
    if(!source||!output||capacity<strlen(source)+64)return false;
    for(unsigned i=0;i<4;++i){
        const char *key=strstr(source,keys[i]);
        if(!key)return false;
        const char *start=key+strlen(keys[i]),*end=start;
        if(*end=='-')++end;
        const char *digits=end;
        while(*end>='0'&&*end<='9')++end;
        if(end==digits||(*end!=','&&*end!='}'))return false;
        fields[i].start=start;fields[i].end=end;
    }
    snprintf(fields[0].value,sizeof(fields[0].value),"%lu",(unsigned long)revision);
    snprintf(fields[1].value,sizeof(fields[1].value),"%d",mirror);
    snprintf(fields[2].value,sizeof(fields[2].value),"%d",text_top);
    snprintf(fields[3].value,sizeof(fields[3].value),"%d",end_scroll);
    const char *cursor=source;char *destination=output;
    for(unsigned n=0;n<4;++n){
        unsigned next=4;
        for(unsigned i=0;i<4;++i)
            if(fields[i].start>=cursor&&(next==4||fields[i].start<fields[next].start))next=i;
        if(next==4)return false;
        size_t length=fields[next].start-cursor;
        memcpy(destination,cursor,length);destination+=length;
        length=strlen(fields[next].value);memcpy(destination,fields[next].value,length);destination+=length;
        cursor=fields[next].end;
    }
    strcpy(destination,cursor);
    return true;
}
