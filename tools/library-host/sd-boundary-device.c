/* Explicitly selected diagnostic build only. Never ship this image.
 * Commands require a complete RAM + USB snapshot before formatting this card.
 * Real media stays installed; normal HTTP transactions exercise the real VFS.
 * Reset injection interrupts a journal transaction, not the SD power rail. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_system.h"
#include "tinyusb_cdc_acm.h"
static int boundary_rename(const char *from,const char *to);
#define rename boundary_rename
#include "../../firmware/p4/main/script_library.c"
#undef rename

typedef struct { char *path; unsigned char *data; size_t size; bool directory; } snapshot_file_t;
static snapshot_file_t saved[512];
static unsigned saved_count;
static size_t saved_bytes;
static bool snapshot_complete;
static int cut_at,rename_step;
static atomic_bool boundary_busy;
static char restore_order[4097];
static size_t restore_order_size;

static int boundary_rename(const char *from,const char *to)
{
    int result=rename(from,to);
    if(!result&&cut_at&&++rename_step==cut_at){
        printf("\nSD_BOUNDARY_CUT step=%d\n",rename_step);fflush(stdout);
        esp_restart();
    }
    return result;
}
static void dump_hex(const void *raw,size_t size)
{
    const unsigned char *data=raw;for(size_t i=0;i<size;++i)printf("%02x",data[i]);
}
static void clear_snapshot(void)
{
    for(unsigned i=0;i<saved_count;++i){free(saved[i].path);free(saved[i].data);}
    saved_count=0;saved_bytes=0;snapshot_complete=false;
}
static bool snapshot_tree(const char *path,unsigned depth)
{
    if(depth>12)return false;
    DIR *dir=opendir(path);if(!dir)return false;
    struct dirent *entry;bool ok=true;
    while(ok){
        errno=0;entry=readdir(dir);if(!entry){if(errno)ok=false;break;}
        if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
        char child[512];
        if(snprintf(child,sizeof(child),"%s/%s",path,entry->d_name)>=(int)sizeof(child)||saved_count>=512){ok=false;break;}
        struct stat st;if(stat(child,&st)||st.st_size<0){ok=false;break;}
        bool directory=S_ISDIR(st.st_mode);
        if(!directory&&(!S_ISREG(st.st_mode)||saved_bytes+(size_t)st.st_size>4*1024*1024)){ok=false;break;}
        snapshot_file_t *item=&saved[saved_count++];
        item->path=strdup(child);item->directory=directory;item->size=directory?0:(size_t)st.st_size;
        if(!item->path){ok=false;break;}
        if(directory){
            printf("\nSD_SNAPSHOT_DIR ");dump_hex(child,strlen(child));printf("\n");
            ok=snapshot_tree(child,depth+1);continue;
        }
        item->data=malloc(item->size?item->size:1);if(!item->data){ok=false;break;}
        FILE *file=fopen(child,"rb");if(!file){ok=false;break;}
        bool read_ok=fread(item->data,1,item->size,file)==item->size&&!ferror(file);
        if(fclose(file)||!read_ok){ok=false;break;}
        saved_bytes+=item->size;
        printf("\nSD_SNAPSHOT_FILE ");dump_hex(child,strlen(child));printf(" %u\n",(unsigned)item->size);
        for(size_t at=0;at<item->size;at+=256){
            size_t n=item->size-at;if(n>256)n=256;
            printf("SD_SNAPSHOT_DATA %u ",(unsigned)at);dump_hex(item->data+at,n);printf("\n");fflush(stdout);vTaskDelay(1);
        }
        printf("SD_SNAPSHOT_END\n");fflush(stdout);
    }
    if(closedir(dir))ok=false;
    return ok;
}
static bool restore_snapshot(void)
{
    bool ok=true;
    for(unsigned i=0;i<saved_count&&ok;++i){
        snapshot_file_t *item=&saved[i];
        if(item->directory){if(mkdir(item->path,0777)&&errno!=EEXIST)ok=false;continue;}
        FILE *f=fopen(item->path,"wb");if(!f){ok=false;break;}
        ok=fwrite(item->data,1,item->size,f)==item->size;
        if(fflush(f)||fsync(fileno(f)))ok=false;
        if(fclose(f))ok=false;
        if(!ok)break;
        f=fopen(item->path,"rb");if(!f){ok=false;break;}
        unsigned char data[512];size_t at=0,n;
        while((n=fread(data,1,sizeof(data),f))){
            if(at+n>item->size||memcmp(data,item->data+at,n)){ok=false;break;}at+=n;
        }
        if(ferror(f)||at!=item->size)ok=false;
        if(fclose(f))ok=false;
    }
    return ok;
}
static void boundary_worker(void *arg)
{
    const char *command=arg;bool ok=false;
    crash_log_stage(TP_TRACE_SD_TEST,!strcmp(command,"backup")?1:!strcmp(command,"fat32")?2:!strcmp(command,"exfat")?3:!strcmp(command,"fill")?4:5);
    if(!script_library_maintenance()&&xSemaphoreTakeRecursive(mutex,pdMS_TO_TICKS(5000))==pdTRUE){
        prompter_status_t state;prompter_snapshot(&state,NULL);
        if(card&&mount_error==ESP_OK&&!internal_storage&&!state.playing&&!firmware_update_in_progress()){
            atomic_store(&maintenance,true);
            BYTE pdrv=ff_diskio_get_pdrv_card(card);char drive[8];snprintf(drive,sizeof(drive),"%u:",pdrv);
            FATFS *fs=NULL;DWORD available=0;FRESULT result=f_getfree(drive,&available,&fs);
            uint64_t capacity=(uint64_t)card->csd.capacity*card->csd.sector_size;
            printf("\nSD_BOUNDARY_GATE op=%s free_result=%d capacity=%llu snapshot=%d files=%u\n",command,result,(unsigned long long)capacity,snapshot_complete,saved_count);fflush(stdout);
            if(result==FR_OK&&capacity>60000000000ULL&&capacity<65000000000ULL){
                if(!strcmp(command,"backup")){
                    clear_snapshot();ok=snapshot_tree("/sdcard",0);snapshot_complete=ok;
                    printf("\nSD_SNAPSHOT_COMPLETE ok=%d entries=%u bytes=%u\n",ok,saved_count,(unsigned)saved_bytes);
                }else if(!strcmp(command,"fat32")||!strcmp(command,"exfat")){
                    if(snapshot_complete){
                        bool fat32=!strcmp(command,"fat32");
                        MKFS_PARM parameters={.fmt=fat32?FM_FAT32:FM_EXFAT,.au_size=fat32?32768:131072};
                        void *work=heap_caps_malloc(65536,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
                        if(work){
                            result=f_mount(NULL,drive,0);
                            if(result==FR_OK)result=f_mkfs(drive,&parameters,work,65536);
                            FRESULT mounted=f_mount(fs,drive,1);free(work);
                            ok=result==FR_OK&&mounted==FR_OK&&fs->fs_type==(fat32?FS_FAT32:FS_EXFAT);
                            if(ok)ok=restore_snapshot();
                            if(ok){io_failed=sd_io_fault=storage_full=false;sd_filesystem=fat32?"fat32":"exfat";++storage_generation;ok=choose_library(false)==ESP_OK;}
                            printf("\nSD_BOUNDARY_FORMAT result=%d mount=%d fs=%u restored=%d\n",result,mounted,fs->fs_type,ok);
                        }
                    }
                }else if(!strcmp(command,"fill")){
                    ok=true;
                    for(unsigned i=0;i<128&&available&&ok;++i){
                        char path[48];snprintf(path,sizeof(path),"%s/.tpfill%02u",drive,i);
                        /* IDF's dynamic per-file sector cache requires a blank
                         * FIL (the normal VFS allocates it with calloc). */
                        FIL file={0};result=f_open(&file,path,FA_WRITE|FA_CREATE_NEW);
                        if(result!=FR_OK){ok=false;break;}
                        uint64_t bytes=(uint64_t)available*fs->csize*512;
                        if(bytes>0x40000000ULL)bytes=0x40000000ULL;
                        printf("SD_BOUNDARY_ALLOC file=%u bytes=%llu stack=%u\n",i,(unsigned long long)bytes,(unsigned)uxTaskGetStackHighWaterMark(NULL));fflush(stdout);
                        tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0,pdMS_TO_TICKS(100));vTaskDelay(pdMS_TO_TICKS(20));
                        /* Free space can be fragmented by normal journal
                         * writes. Expand requires a contiguous extent; keep
                         * splitting the last extent down to one real cluster. */
                        do{
                            result=f_expand(&file,(FSIZE_t)bytes,1);
                            if(result!=FR_DENIED||bytes==(uint64_t)fs->csize*512)break;
                            uint64_t clusters=bytes/((uint64_t)fs->csize*512);
                            bytes=(clusters/2)*(uint64_t)fs->csize*512;
                        }while(bytes);
                        FRESULT closed=f_close(&file);
                        ok=result==FR_OK&&closed==FR_OK;
                        if(ok)ok=f_getfree(drive,&available,&fs)==FR_OK;
                        printf("SD_BOUNDARY_FILL file=%u free_clusters=%lu result=%d\n",i,(unsigned long)available,result);fflush(stdout);vTaskDelay(1);
                    }
                    ok=ok&&available==0;
                }else if(!strcmp(command,"clear")){
                    ok=true;
                    for(unsigned i=0;i<128;++i){char path[48];snprintf(path,sizeof(path),"%s/.tpfill%02u",drive,i);result=f_unlink(path);if(result!=FR_OK&&result!=FR_NO_FILE)ok=false;}
                    io_failed=sd_io_fault=storage_full=false;
                    printf("SD_BOUNDARY_CLEAR ok=%d\n",ok);
                }else if(!strcmp(command,"restoreorder")&&snapshot_complete){
                    cJSON *order=cJSON_ParseWithOpts(restore_order,NULL,true);ok=cJSON_IsArray(order);
                    const cJSON *item;
                    cJSON_ArrayForEach(item,order)if(!cJSON_IsString(item)||!identity_valid(item->valuestring))ok=false;
                    if(ok)ok=write_order(order);
                    cJSON_Delete(order);
                }
            }
            atomic_store(&maintenance,false);
        }
        xSemaphoreGiveRecursive(mutex);
    }
    printf("\nSD_BOUNDARY_DONE command=%s ok=%d\n",command,ok);fflush(stdout);
    free(arg);atomic_store(&boundary_busy,false);vTaskDelete(NULL);
}
void tp_sd_boundary_command(const char *command)
{
    if(!strcmp(command,"ordreset")){
        if(!atomic_load(&boundary_busy)){restore_order_size=0;restore_order[0]=0;}
        return;
    }
    if(!strncmp(command,"ord+",4)){
        if(atomic_load(&boundary_busy))return;
        const char *hex=command+4;size_t n=strlen(hex);
        bool valid=!(n&1)&&restore_order_size+n/2<sizeof(restore_order);
        for(size_t i=0;valid&&i<n;i+=2){
            unsigned value=0;
            for(unsigned j=0;j<2;++j){char c=hex[i+j];if(c>='0'&&c<='9')value=value*16+c-'0';else if(c>='a'&&c<='f')value=value*16+c-'a'+10;else valid=false;}
            if(valid)restore_order[restore_order_size++]=(char)value;
        }
        if(!valid)restore_order_size=0;
        restore_order[restore_order_size]=0;return;
    }
    if(!strcmp(command,"panic")){
        if(!atomic_load(&boundary_busy)&&!firmware_update_in_progress()){
            prompter_status_t state;prompter_snapshot(&state,NULL);
            if(!state.playing&&!state.countdown_remaining_ms){
                crash_log_stage(TP_TRACE_SD_TEST,90);
                abort();
            }
        }
        return;
    }
    if(!strncmp(command,"cut",3)){
        if(!atomic_load(&boundary_busy)){cut_at=atoi(command+3);rename_step=0;printf("\nSD_BOUNDARY_ARM step=%d\n",cut_at);fflush(stdout);}
        return;
    }
    if(strcmp(command,"backup")&&strcmp(command,"fat32")&&strcmp(command,"exfat")&&strcmp(command,"fill")&&strcmp(command,"clear")&&strcmp(command,"restoreorder"))return;
    if(atomic_exchange(&boundary_busy,true))return;
    char *copy=strdup(command);
    if(!copy||xTaskCreate(boundary_worker,"sd_boundary",16384,copy,1,NULL)!=pdPASS){free(copy);atomic_store(&boundary_busy,false);}
}
