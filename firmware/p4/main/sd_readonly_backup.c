/* Diagnostic builds only. The disk driver separately blocks writes and TRIM.
 * Private file names/content go only to the local USB console, never to an API.
 * A capped/incomplete traversal must never be treated as a complete backup. */
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static unsigned files;
static uint64_t bytes;
static void hex(const void *data,size_t count)
{
    const unsigned char *p=data;
    for(size_t i=0;i<count;++i)printf("%02x",p[i]);
}
static bool visit(const char *path,unsigned depth)
{
    if(depth>12)return false;
    DIR *dir=opendir(path);if(!dir)return false;
    struct dirent *entry;bool ok=true;
    while(ok){
        errno=0;entry=readdir(dir);
        if(!entry){if(errno)ok=false;break;}
        if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
        char child[512];
        if(snprintf(child,sizeof(child),"%s/%s",path,entry->d_name)>=(int)sizeof(child)){ok=false;break;}
        struct stat st;if(stat(child,&st)){ok=false;break;}
        if(S_ISDIR(st.st_mode)){ok=visit(child,depth+1);continue;}
        if(!S_ISREG(st.st_mode)||st.st_size<0||bytes+(uint64_t)st.st_size>16*1024*1024){ok=false;break;}
        FILE *file=fopen(child,"rb");if(!file){ok=false;break;}
        printf("\nSD_BACKUP_FILE ");hex(child,strlen(child));printf(" %" PRIu64 "\n",(uint64_t)st.st_size);
        unsigned char buffer[256];size_t n;uint64_t offset=0;
        while((n=fread(buffer,1,sizeof(buffer),file))){
            printf("SD_BACKUP_DATA %" PRIu64 " ",offset);hex(buffer,n);printf("\n");
            fflush(stdout);offset+=n;vTaskDelay(1);
        }
        if(ferror(file)||offset!=(uint64_t)st.st_size)ok=false;
        if(fclose(file))ok=false;
        printf("SD_BACKUP_END %d\n",ok);++files;bytes+=offset;
    }
    if(closedir(dir))ok=false;
    return ok;
}
esp_err_t tp_sd_readonly_backup(void)
{
    files=0;bytes=0;
    printf("\nSD_BACKUP_BEGIN read_only=1\n");
    bool ok=visit("/sdcard",0);
    printf("SD_BACKUP_COMPLETE ok=%d files=%u bytes=%" PRIu64 "\n",ok,files,bytes);
    fflush(stdout);
    return ok?ESP_OK:ESP_FAIL;
}
