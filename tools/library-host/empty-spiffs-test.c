#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "empty_spiffs_image.h"

static bool recognise(const unsigned char *image,size_t length)
{
    tp_empty_spiffs_t scan;tp_empty_spiffs_begin(&scan,length);
    for(size_t i=0;i<length;i+=4096)tp_empty_spiffs_block(&scan,image+i,4096);
    return tp_empty_spiffs_complete(&scan);
}
int main(void)
{
    size_t bytes=0x3f0000;unsigned char *data=malloc(bytes);assert(data);
    memset(data,255,bytes);assert(!recognise(data,bytes));
    for(unsigned count=3;count<=1008;count+=1){
        memset(data,255,bytes);
        for(unsigned i=0;i<count;i++){
            unsigned magic=(0x20140529^256^(count-i))&65535;
            data[i*4096+252]=magic;data[i*4096+253]=magic>>8;
        }
        assert(recognise(data,bytes));
        data[(count-1)*4096+300]=0;assert(!recognise(data,bytes));
        data[(count-1)*4096+300]=255;
        data[0]=0;assert(!recognise(data,bytes));data[0]=255;
        data[252]^=1;assert(!recognise(data,bytes));
    }
    memset(data,255,bytes);assert(!recognise(data,bytes));
    tp_empty_spiffs_t partial;tp_empty_spiffs_begin(&partial,bytes);
    assert(!tp_empty_spiffs_complete(&partial));
    tp_empty_spiffs_begin(&partial,bytes-1);assert(!partial.valid);
    free(data);puts("PASS exact empty SPIFFS geometry, occupied/damaged rejection and incomplete scans");
}
