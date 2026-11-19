#include "script_library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "cJSON.h"
#ifndef TP_LIBRARY_HOST_TEST
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "diskio_sdmmc.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "device_settings.h"
#include "teleprompter_ui.h"
#include "firmware_update.h"
#include "factory_reset.h"
#include "wifi_settings.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "esp_random.h"
#include "psa/crypto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdatomic.h>
#else
#include "library_host_shim.h"
#include <openssl/sha.h>
#endif

#ifndef ROOT
#define ROOT "/sdcard/teleprompter"
#endif
#define TITLE_MAX 96
#define STYLE_MASK (TP_SPEED|TP_FONT|TP_ROTATION|TP_LETTER_SPACING|TP_LINE_SPACING|TP_COUNTDOWN|TP_LOOP|TP_AUTONEXT|TP_BOLD|TP_SHOW_INFO|TP_TEXT_ALIGN|TP_MARGIN_LEFT|TP_MARGIN_RIGHT|TP_TEXT_COLOR|TP_MIRROR|TP_SIGHT_GUIDE|TP_CENTER_HIGHLIGHT)
static SemaphoreHandle_t mutex;
static sdmmc_card_t *card;
static esp_ldo_channel_handle_t sd_power;
static bool spi_ready;
static bool mount_attempted;
static bool ejected;
static char active[TITLE_MAX+1];
#ifndef TP_LIBRARY_HOST_TEST
static atomic_uint revision=1;
#else
static uint32_t revision=1;
#endif
uint32_t script_library_revision(void) { return revision; }
static esp_err_t mount_error=ESP_ERR_NOT_FOUND;
static prompter_status_t defaults;
static const char *TAG="tp_library";
#ifndef TP_LIBRARY_HOST_TEST
static TaskHandle_t playback_worker;
static atomic_uint advance_revision;
static atomic_uint startup_stage;
static atomic_bool maintenance;
static char sd_operation_error[48];
static portMUX_TYPE next_title_lock=portMUX_INITIALIZER_UNLOCKED;
static char next_title[TITLE_MAX+1];
static char previous_title[TITLE_MAX+1];
void script_library_previous_title(char *title,size_t size)
{
    portENTER_CRITICAL(&next_title_lock);
    snprintf(title,size,"%s",previous_title);
    portEXIT_CRITICAL(&next_title_lock);
}
void script_library_next_title(char *title,size_t size)
{
    portENTER_CRITICAL(&next_title_lock);
    snprintf(title,size,"%s",next_title);
    portEXIT_CRITICAL(&next_title_lock);
}
bool script_library_maintenance(void){return atomic_load(&maintenance);}
unsigned script_library_startup_stage(void){return atomic_load(&startup_stage);}
void script_library_startup_begin(void){if(playback_worker)xTaskNotifyGive(playback_worker);}
static void library_playback_worker(void *arg);
#endif

static bool title_valid(const char *s)
{
    size_t n=s?strlen(s):0;
    if(!n||n>TITLE_MAX||s[0]=='.'||s[0]==' '||s[n-1]==' '||s[n-1]=='.'||!prompter_text_valid(s,n))return false;
    for(size_t i=0;i<n;i++)if((unsigned char)s[i]<32||strchr("/\\:*?\"<>|",s[i]))return false;
    return true;
}
static void path_for(char *path,const char *title,const char *ext){snprintf(path,256,ROOT"/%s.%s",title,ext);}
static char *read_file(const char *path,size_t limit,size_t *length)
{
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);
    if(n<0||(size_t)n>limit||fseek(f,0,SEEK_SET)){fclose(f);return NULL;}
    char *p=malloc(n+1);if(!p){fclose(f);return NULL;}
    size_t got=fread(p,1,n,f);bool ok=got==(size_t)n&&!ferror(f);fclose(f);
    if(!ok){free(p);return NULL;}p[n]=0;if(length)*length=n;return p;
}
static bool write_file(const char *path,const char *text)
{
    FILE *f=fopen(path,"wb");if(!f)return false;
    size_t n=strlen(text);bool ok=fwrite(text,1,n,f)==n;
    if(fflush(f)||fsync(fileno(f)))ok=false;
    if(fclose(f))ok=false;
    return ok;
}
static bool exists(const char *path){struct stat st;return stat(path,&st)==0;}
static void identity_new(char id[33])
{
    unsigned char bytes[16];
#ifdef TP_LIBRARY_HOST_TEST
    FILE *f=fopen("/dev/urandom","rb");if(!f)abort();if(fread(bytes,1,sizeof(bytes),f)!=sizeof(bytes))abort();fclose(f);
#else
    esp_fill_random(bytes,sizeof(bytes));
#endif
    for(int i=0;i<16;i++)snprintf(id+i*2,3,"%02x",bytes[i]);
}
static bool identity_valid(const char *id)
{
    if(!id||strlen(id)!=32)return false;
    for(int i=0;i<32;i++)if(!strchr("0123456789abcdef",id[i]))return false;
    return true;
}
static void content_hash(const char *text,char hash[65])
{
    unsigned char bytes[32];
#ifdef TP_LIBRARY_HOST_TEST
    SHA256((const unsigned char*)text,strlen(text),bytes);
#else
    size_t length=0;
    if(psa_crypto_init()!=PSA_SUCCESS||psa_hash_compute(PSA_ALG_SHA_256,(const unsigned char*)text,strlen(text),bytes,sizeof(bytes),&length)!=PSA_SUCCESS||length!=sizeof(bytes)){
        hash[0]=0;return;
    }
#endif
    for(int i=0;i<32;i++)snprintf(hash+i*2,3,"%02x",bytes[i]);
}

/* Metadata is separate from the optional style and is committed with it.
 * Legacy files keep an unknown creation time; listing never modifies the card. */
static bool timestamp_valid(double stamp){return stamp>=1577836800.0&&stamp<4102444800.0;}
static void sync_clock(const cJSON *q)
{
#ifndef TP_LIBRARY_HOST_TEST
    const cJSON *zone=cJSON_GetObjectItem(q,"timezoneOffset");
    if(cJSON_IsNumber(zone)&&zone->valuedouble==zone->valueint)device_settings_clock_hint(zone->valueint);
    const cJSON *stamp=cJSON_GetObjectItem(q,"timestamp");
    if(!timestamp_valid((double)time(NULL))&&cJSON_IsNumber(stamp)&&timestamp_valid(stamp->valuedouble)){
        struct timeval tv={.tv_sec=(time_t)stamp->valuedouble};settimeofday(&tv,NULL);
    }
#else
    (void)q;
#endif
}
static size_t character_count(const char *text)
{
    size_t count=0;const unsigned char *p=(const unsigned char*)text;
    while(*p){
        unsigned cp=*p++;int more=0;
        if(cp>=0xf0){cp&=7;more=3;}else if(cp>=0xe0){cp&=15;more=2;}else if(cp>=0xc0){cp&=31;more=1;}
        while(more--&&*p)cp=(cp<<6)|(*p++&63);
        bool space=(cp>=9&&cp<=13)||cp==32||cp==0xa0||cp==0x1680||(cp>=0x2000&&cp<=0x200a)||cp==0x2028||cp==0x2029||cp==0x202f||cp==0x205f||cp==0x3000||cp==0xfeff;
        if(!space)count++;
    }return count;
}
static unsigned char search_lower(unsigned char c){return c>='A'&&c<='Z'?c+('a'-'A'):c;}
static bool search_contains(const char *text,const char *term,size_t length)
{
    if(!text)return false;
    for(const unsigned char *p=(const unsigned char *)text;*p;p++){
        size_t i=0;while(i<length&&p[i]&&search_lower(p[i])==search_lower((unsigned char)term[i]))i++;
        if(i==length)return true;
    }
    return false;
}
static bool search_matches(const char *title,const char *body,const char *query)
{
    while(*query){
        while(*query==' '||*query=='\t'||*query=='\n'||*query=='\r')query++;
        const char *end=query;while(*end&&*end!=' '&&*end!='\t'&&*end!='\n'&&*end!='\r')end++;
        size_t n=end-query;if(n&&!search_contains(title,query,n)&&!search_contains(body,query,n))return false;
        query=end;
    }
    return true;
}
static void add_excerpt(cJSON *item,const char *text)
{
    char excerpt[96*4+1];size_t offset=0,characters=0;
    if(text&&prompter_text_valid(text,strlen(text))){
        while(text[offset]&&characters<96){
            unsigned char lead=(unsigned char)text[offset];
            size_t width=lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;
            memcpy(excerpt+offset,text+offset,width);offset+=width;++characters;
        }
    }
    excerpt[offset]=0;cJSON_AddStringToObject(item,"excerpt",excerpt);
}
static cJSON *read_metadata(const char *title)
{
    char path[256];path_for(path,title,"tpmeta");char *raw=read_file(path,512,NULL);
    cJSON *j=raw?cJSON_Parse(raw):NULL;free(raw);
    if(!cJSON_IsObject(j)){cJSON_Delete(j);return cJSON_CreateObject();}return j;
}
static void add_metadata(cJSON *item,const char *title,const char *text)
{
    cJSON *meta=read_metadata(title);
    const char *keys[]={"createdAt","updatedAt"};
    for(int i=0;i<2;i++){
        const cJSON *stamp=cJSON_GetObjectItem(meta,keys[i]);
        if(cJSON_IsNumber(stamp)&&timestamp_valid(stamp->valuedouble))cJSON_AddNumberToObject(item,keys[i],stamp->valuedouble);
        else cJSON_AddNullToObject(item,keys[i]);
    }
    if(text)cJSON_AddNumberToObject(item,"characters",character_count(text));
    else cJSON_AddNullToObject(item,"characters");
    cJSON_Delete(meta);
}
static char *next_metadata(const char *title,const char *text,bool is_new)
{
    cJSON *j=read_metadata(title);double now=(double)time(NULL);
    const cJSON *id=cJSON_GetObjectItem(j,"id");
    if(!cJSON_IsString(id)||!identity_valid(id->valuestring)){
        char uuid[33];identity_new(uuid);cJSON_DeleteItemFromObject(j,"id");cJSON_AddStringToObject(j,"id",uuid);
    }
    char digest[65];content_hash(text,digest);cJSON_DeleteItemFromObject(j,"contentHash");cJSON_AddStringToObject(j,"contentHash",digest);
    if(is_new){cJSON_DeleteItemFromObject(j,"createdAt");if(timestamp_valid(now))cJSON_AddNumberToObject(j,"createdAt",now);}
    if(timestamp_valid(now)){cJSON_DeleteItemFromObject(j,"updatedAt");cJSON_AddNumberToObject(j,"updatedAt",now);}
    cJSON_DeleteItemFromObject(j,"characters");cJSON_AddNumberToObject(j,"characters",character_count(text));
    char *raw=cJSON_PrintUnformatted(j);cJSON_Delete(j);return raw;
}

/* A single journal covers text, optional style and metadata. Old two-file
 * journals remain readable. An interruption restores the original set. */
static bool recover(void)
{
    char *raw=read_file(ROOT"/.transaction",512,NULL);if(!raw)return !exists(ROOT"/.transaction");
    cJSON *j=cJSON_Parse(raw);free(raw);
    cJSON *name=cJSON_GetObjectItem(j,"title"),*flags=cJSON_GetObjectItem(j,"existed");
    if(!cJSON_IsString(name)||!title_valid(name->valuestring)||!cJSON_IsNumber(flags)){cJSON_Delete(j);return false;}
    const char *exts[]={"txt","json","tpmeta"};bool ok=true;
    const cJSON *files=cJSON_GetObjectItem(j,"files");int count=cJSON_IsNumber(files)&&files->valueint==3?3:2;
    for(int i=0;i<count;i++){
        char path[256],backup[272];path_for(path,name->valuestring,exts[i]);snprintf(backup,sizeof(backup),"%s.tpbak",path);
        if(exists(backup)){remove(path);if(rename(backup,path))ok=false;}
        else if(!(flags->valueint&(1<<i)))remove(path);
    }
    cJSON_Delete(j);if(ok)remove(ROOT"/.transaction");return ok;
}
static bool transaction(const char *title,const char *text,const char *style,bool deleting)
{
    if(!recover())return false;
    char paths[3][256],backups[3][272],temps[3][272];const char *exts[]={"txt","json","tpmeta"};
    char original[256];path_for(original,title,"txt");
    char *metadata=deleting?NULL:next_metadata(title,text,!exists(original));
    if(!deleting&&!metadata)return false;
    const char *data[]={text,style,metadata};int flags=0;
    for(int i=0;i<3;i++){
        path_for(paths[i],title,exts[i]);snprintf(backups[i],272,"%.255s.tpbak",paths[i]);snprintf(temps[i],272,"%.255s.tpnew",paths[i]);
        if(exists(paths[i]))flags|=1<<i;
        remove(backups[i]);remove(temps[i]);
        if(!deleting&&data[i]&&!write_file(temps[i],data[i])){free(metadata);return false;}
    }
    cJSON *journal=cJSON_CreateObject();cJSON_AddStringToObject(journal,"title",title);cJSON_AddNumberToObject(journal,"existed",flags);cJSON_AddNumberToObject(journal,"files",3);
    char *raw=cJSON_PrintUnformatted(journal);cJSON_Delete(journal);
    bool ok=raw&&write_file(ROOT"/.transaction.new",raw);free(raw);
    if(!ok||rename(ROOT"/.transaction.new",ROOT"/.transaction")){free(metadata);return false;}
    for(int i=0;i<3&&ok;i++){
        if((flags&(1<<i))&&rename(paths[i],backups[i]))ok=false;
        if(ok&&!deleting&&data[i]&&rename(temps[i],paths[i]))ok=false;
    }
    free(metadata);
    if(!ok){recover();return false;}
    if(remove(ROOT"/.transaction")){recover();return false;}
    for(int i=0;i<3;i++){remove(backups[i]);remove(temps[i]);}
    ++revision;return true;
}
static void file_revision(const char *text,size_t size,const char *style,char out[17])
{
    uint64_t hash=14695981039346656037ULL;
    for(size_t i=0;i<size;i++){hash^=(unsigned char)text[i];hash*=1099511628211ULL;}
    hash^=255;hash*=1099511628211ULL;
    if(style)for(const unsigned char *p=(const unsigned char*)style;*p;p++){hash^=*p;hash*=1099511628211ULL;}
    snprintf(out,17,"%016llx",(unsigned long long)hash);
}
/* Custom order lives on SD separately from manuscripts and their timestamps. */
static bool recover_order(void)
{
    if(!exists(ROOT"/.order.json")&&exists(ROOT"/.order.bak")){
        if(rename(ROOT"/.order.bak",ROOT"/.order.json"))return false;
    }
    if(exists(ROOT"/.order.json"))remove(ROOT"/.order.bak");
    remove(ROOT"/.order.tmp");return true;
}
static cJSON *read_order(void)
{
    char *raw=read_file(ROOT"/.order.json",57344,NULL);
    cJSON *order=raw?cJSON_Parse(raw):NULL;free(raw);
    if(!cJSON_IsArray(order)){cJSON_Delete(order);return cJSON_CreateArray();}
    return order;
}
static int order_rank(const cJSON *order,const char *title)
{
    int i=0;const cJSON *item;
    cJSON_ArrayForEach(item,order){if(cJSON_IsString(item)&&!strcmp(item->valuestring,title))return i;++i;}
    return -1;
}
static int compare_names(const void *a,const void *b){return strcmp(*(char *const*)a,*(char *const*)b);}
static cJSON *current_titles(void)
{
    DIR *dir=opendir(ROOT);if(!dir)return NULL;
    char **names=NULL;size_t count=0;struct dirent *entry;bool valid=true;
    while((entry=readdir(dir))){
        size_t n=strlen(entry->d_name);
        if(n<5||n-4>TITLE_MAX||strcasecmp(entry->d_name+n-4,".txt"))continue;
        char title[TITLE_MAX+1],path[256];memcpy(title,entry->d_name,n-4);title[n-4]=0;
        if(!title_valid(title))continue;
        path_for(path,title,"txt");struct stat st;if(stat(path,&st)||!S_ISREG(st.st_mode))continue;
        char **next=realloc(names,(count+1)*sizeof(*names));if(!next){valid=false;break;}names=next;
        names[count]=strdup(title);if(!names[count]){valid=false;break;}count++;
    }
    closedir(dir);if(count>1)qsort(names,count,sizeof(*names),compare_names);
    cJSON *titles=valid?cJSON_CreateArray():NULL;
    for(size_t i=0;i<count;i++){if(titles)cJSON_AddItemToArray(titles,cJSON_CreateString(names[i]));free(names[i]);}free(names);
    return titles;
}
static bool save_identity(const char *title,const cJSON *meta)
{
    char path[256],temp[272],backup[272];path_for(path,title,"tpmeta");
    snprintf(temp,sizeof(temp),"%s.idnew",path);snprintf(backup,sizeof(backup),"%s.idbak",path);
    if(!exists(path)&&exists(backup)&&rename(backup,path))return false;
    char *raw=cJSON_PrintUnformatted(meta);if(!raw)return false;
    bool ok=write_file(temp,raw);free(raw);if(!ok)return false;
    remove(backup);bool previous=exists(path);
    if(previous&&rename(path,backup))return false;
    if(rename(temp,path)){if(previous)rename(backup,path);return false;}
    remove(backup);return true;
}
static bool prepare_identities(const cJSON *titles)
{
    cJSON *used=cJSON_CreateArray(),*live=cJSON_CreateArray();if(!used||!live){cJSON_Delete(used);cJSON_Delete(live);return false;}
    const cJSON *name;bool ok=true;
    cJSON_ArrayForEach(name,titles){
        char path[256],backup[272];path_for(path,name->valuestring,"tpmeta");snprintf(backup,sizeof(backup),"%s.idbak",path);
        if(!exists(path)&&exists(backup)&&rename(backup,path)){cJSON_Delete(used);cJSON_Delete(live);return false;}
        cJSON *meta=read_metadata(name->valuestring);const cJSON *id=cJSON_GetObjectItem(meta,"id");
        if(cJSON_IsString(id)&&identity_valid(id->valuestring))cJSON_AddItemToArray(live,cJSON_CreateString(id->valuestring));
        cJSON_Delete(meta);
    }
    cJSON_ArrayForEach(name,titles){
        const char *title=name->valuestring;char path[256],backup[272];path_for(path,title,"tpmeta");
        snprintf(backup,sizeof(backup),"%s.idbak",path);
        if(!exists(path)&&exists(backup)&&rename(backup,path)){ok=false;break;}
        cJSON *meta=read_metadata(title);const cJSON *id=cJSON_GetObjectItem(meta,"id");
        bool known=cJSON_IsString(id)&&identity_valid(id->valuestring)&&order_rank(used,id->valuestring)<0;
        if(known){cJSON_AddItemToArray(used,cJSON_CreateString(id->valuestring));cJSON_Delete(meta);continue;}
        char *body;path_for(path,title,"txt");body=read_file(path,PROMPTER_MAX_BYTES,NULL);
        char digest[65]={0};if(body){content_hash(body,digest);free(body);}
        /* An external TXT-only rename is recoverable only with one orphan metadata
         * match and no other live manuscript with identical contents. */
        bool duplicate=false;const cJSON *other;
        cJSON_ArrayForEach(other,titles){
            if(!strcmp(other->valuestring,title))continue;
            path_for(path,other->valuestring,"txt");body=read_file(path,PROMPTER_MAX_BYTES,NULL);
            if(body){char hash[65];content_hash(body,hash);free(body);if(!strcmp(hash,digest)){duplicate=true;break;}}
        }
        cJSON *candidate=NULL;int matches=0;DIR *dir=digest[0]?opendir(ROOT):NULL;struct dirent *entry;
        while(dir&&(entry=readdir(dir))){
            size_t n=strlen(entry->d_name);if(n<8||n-7>TITLE_MAX||strcmp(entry->d_name+n-7,".tpmeta"))continue;
            char old[TITLE_MAX+1];memcpy(old,entry->d_name,n-7);old[n-7]=0;
            if(!title_valid(old)||order_rank(titles,old)>=0)continue;
            cJSON *orphan=read_metadata(old);const cJSON *hash=cJSON_GetObjectItem(orphan,"contentHash"),*oid=cJSON_GetObjectItem(orphan,"id");
            if(cJSON_IsString(hash)&&!strcmp(hash->valuestring,digest)&&cJSON_IsString(oid)&&identity_valid(oid->valuestring)&&order_rank(used,oid->valuestring)<0&&order_rank(live,oid->valuestring)<0){
                matches++;cJSON_Delete(candidate);candidate=orphan;
            }else cJSON_Delete(orphan);
        }
        if(dir)closedir(dir);
        if(matches==1&&!duplicate){cJSON_Delete(meta);meta=candidate;candidate=NULL;}
        else{
            bool copied=cJSON_IsString(id)&&identity_valid(id->valuestring);
            char uuid[33];identity_new(uuid);cJSON_DeleteItemFromObject(meta,"id");cJSON_AddStringToObject(meta,"id",uuid);
            if(matches>0||copied){cJSON_DeleteItemFromObject(meta,"identityIssue");cJSON_AddBoolToObject(meta,"identityIssue",true);}
        }
        cJSON_Delete(candidate);cJSON_DeleteItemFromObject(meta,"contentHash");if(digest[0])cJSON_AddStringToObject(meta,"contentHash",digest);
        id=cJSON_GetObjectItem(meta,"id");cJSON_AddItemToArray(used,cJSON_CreateString(id->valuestring));
        ok=save_identity(title,meta);cJSON_Delete(meta);if(!ok)break;
    }
    cJSON_Delete(used);cJSON_Delete(live);return ok;
}
static cJSON *current_ids(const cJSON *titles)
{
    cJSON *ids=cJSON_CreateArray();const cJSON *name;
    cJSON_ArrayForEach(name,titles){
        cJSON *meta=read_metadata(name->valuestring);const cJSON *id=cJSON_GetObjectItem(meta,"id");
        if(!cJSON_IsString(id)){cJSON_Delete(meta);cJSON_Delete(ids);return NULL;}
        cJSON_AddItemToArray(ids,cJSON_CreateString(id->valuestring));cJSON_Delete(meta);
    }return ids;
}
static bool order_tag(const cJSON *titles,const cJSON *order,char tag[17])
{
    char *a=cJSON_PrintUnformatted(titles),*b=cJSON_PrintUnformatted(order);
    if(!a||!b){free(a);free(b);return false;}
    file_revision(a,strlen(a),b,tag);free(a);free(b);return true;
}
static bool write_order(const cJSON *order)
{
    if(!recover_order())return false;
    char *raw=cJSON_PrintUnformatted(order);if(!raw)return false;
    bool ok=write_file(ROOT"/.order.tmp",raw);free(raw);if(!ok)return false;
    bool previous=exists(ROOT"/.order.json");
    if(previous&&rename(ROOT"/.order.json",ROOT"/.order.bak"))return false;
    if(rename(ROOT"/.order.tmp",ROOT"/.order.json")){
        if(previous)rename(ROOT"/.order.bak",ROOT"/.order.json");
        return false;
    }
    remove(ROOT"/.order.bak");++revision;return true;
}
static char *load_text(const char *title,size_t *size,char **style,char hash[17])
{
    char path[256];path_for(path,title,"txt");char *text=read_file(path,PROMPTER_MAX_BYTES,size);
    if(!text||memchr(text,0,*size)||!prompter_text_valid(text,*size)){free(text);return NULL;}
    if(*size>=3&&!memcmp(text,"\xef\xbb\xbf",3)){memmove(text,text+3,*size-2);*size-=3;}
    path_for(path,title,"json");*style=read_file(path,4096,NULL);
    if(!*style&&exists(path)){free(text);return NULL;}
    file_revision(text,*size,*style,hash);return text;
}
typedef struct {const char *key;unsigned mask;size_t offset;bool boolean;} field_t;
#define FIELD(k,m,f,b) {k,m,offsetof(prompter_status_t,f),b}
static const field_t fields[]={
    FIELD("speed",TP_SPEED,speed,false),FIELD("font",TP_FONT,font_size,false),
    FIELD("rotation",TP_ROTATION,rotation,false),
    FIELD("letterSpacing",TP_LETTER_SPACING,letter_spacing,false),FIELD("lineSpacing",TP_LINE_SPACING,line_spacing,false),
    FIELD("countdown",TP_COUNTDOWN,countdown,false),FIELD("loop",TP_LOOP,loop,true),FIELD("autoNext",TP_AUTONEXT,auto_next,true),
    FIELD("bold",TP_BOLD,bold,true),
    FIELD("showInfo",TP_SHOW_INFO,show_info,false),
    FIELD("textAlign",TP_TEXT_ALIGN,text_align,false),FIELD("marginLeft",TP_MARGIN_LEFT,margin_left,false),
    FIELD("marginRight",TP_MARGIN_RIGHT,margin_right,false),FIELD("textColor",TP_TEXT_COLOR,text_color,false),
    FIELD("mirror",TP_MIRROR,mirror,false),FIELD("sightGuide",TP_SIGHT_GUIDE,sight_guide,true),
    FIELD("centerHighlight",TP_CENTER_HIGHLIGHT,center_highlight,true)};
static bool parse_style(const cJSON *obj,prompter_status_t *v,unsigned *mask)
{
    if(!cJSON_IsObject(obj))return false;
    const cJSON *item;*mask=0;
    cJSON_ArrayForEach(item,obj){
        const field_t *field=NULL;
        for(size_t i=0;i<sizeof(fields)/sizeof(fields[0]);i++)if(!strcmp(item->string,fields[i].key))field=&fields[i];
        if(!field)return false;
        char *target=(char*)v+field->offset;
        if(field->boolean){if(!cJSON_IsBool(item))return false;*(bool*)target=cJSON_IsTrue(item);}
        else{if(!cJSON_IsNumber(item)||item->valuedouble!=item->valueint)return false;*(int*)target=item->valueint;}
        *mask|=field->mask;
    }
    prompter_normalize_playback(mask,v);
    return !*mask||prompter_control_validate(*mask,v)==ESP_OK;
}
static cJSON *style_json(const prompter_status_t *v)
{
    cJSON *j=cJSON_CreateObject();if(!j)return NULL;
    for(size_t i=0;i<sizeof(fields)/sizeof(fields[0]);i++){
        const char *p=(const char*)v+fields[i].offset;
        if(fields[i].boolean)cJSON_AddBoolToObject(j,fields[i].key,*(const bool*)p);
        else cJSON_AddNumberToObject(j,fields[i].key,*(const int*)p);
    }return j;
}
static bool effective_style(const char *raw,prompter_status_t *v)
{
    *v=defaults;v->show_info=-1;if(!raw)return true;
    cJSON *j=cJSON_Parse(raw);cJSON *version=cJSON_GetObjectItem(j,"version"),*style=cJSON_GetObjectItem(j,"style");
    unsigned mask;bool ok=cJSON_IsNumber(version)&&version->valueint==1&&parse_style(style,v,&mask);cJSON_Delete(j);return ok;
}
static char *style_document(const prompter_status_t *v)
{
    cJSON *j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"version",1);cJSON_AddItemToObject(j,"style",style_json(v));
    char *raw=cJSON_PrintUnformatted(j);cJSON_Delete(j);return raw;
}
static esp_err_t mount_card(void)
{
    if(ejected)return ESP_ERR_INVALID_STATE;
#ifdef TP_LIBRARY_HOST_TEST
    card=(sdmmc_card_t*)1;mkdir(ROOT,0777);return recover()&&recover_order()?ESP_OK:ESP_FAIL;
#else
    if(card){
        if(mkdir(ROOT,0777)&&errno!=EEXIST)return ESP_FAIL;
        return recover()&&recover_order()?ESP_OK:ESP_FAIL;
    }
    if(!sd_power){esp_ldo_channel_config_t power={.chan_id=4,.voltage_mv=3300};esp_err_t e=esp_ldo_acquire_channel(&power,&sd_power);if(e!=ESP_OK)return e;}
    if(!spi_ready){
        spi_bus_config_t bus={.mosi_io_num=44,.miso_io_num=39,.sclk_io_num=43,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=4096};
        esp_err_t e=spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO);if(e!=ESP_OK)return e;spi_ready=true;
    }
    sdmmc_host_t host=SDSPI_HOST_DEFAULT();host.slot=SPI2_HOST;host.max_freq_khz=10000;
    sdspi_device_config_t slot=SDSPI_DEVICE_CONFIG_DEFAULT();slot.host_id=SPI2_HOST;slot.gpio_cs=42;
    esp_vfs_fat_mount_config_t cfg={.format_if_mount_failed=false,.max_files=8,.allocation_unit_size=16384};
    esp_err_t err=esp_vfs_fat_sdspi_mount("/sdcard",&host,&slot,&cfg,&card);
    if(err!=ESP_OK){card=NULL;return err;}
    if(mkdir(ROOT,0777)&&errno!=EEXIST)return ESP_FAIL;
    if(!recover()||!recover_order())return ESP_FAIL;
    ESP_LOGI(TAG,"SD_READY SPI2 CLK43 CMD44 D039 CS42 directory="ROOT);return ESP_OK;
#endif
}
esp_err_t script_library_storage_lock(void)
{
    xSemaphoreTakeRecursive(mutex,portMAX_DELAY);
    if(!card){mount_attempted=true;mount_error=mount_card();}
    if(mount_error!=ESP_OK)xSemaphoreGiveRecursive(mutex);
    return mount_error;
}
esp_err_t script_library_try_idle_lock(void)
{
#ifndef TP_LIBRARY_HOST_TEST
    if(!mutex||xSemaphoreTakeRecursive(mutex,0)!=pdTRUE)return ESP_ERR_INVALID_STATE;
    if(atomic_load(&maintenance)){xSemaphoreGiveRecursive(mutex);return ESP_ERR_INVALID_STATE;}
#endif
    return ESP_OK;
}
void script_library_storage_unlock(void){xSemaphoreGiveRecursive(mutex);}
esp_err_t script_library_init(void)
{
    mutex=xSemaphoreCreateRecursiveMutex();if(!mutex)return ESP_ERR_NO_MEM;
    defaults=(prompter_status_t){PROMPTER_DEFAULT_CONTROLS};
    prompter_status_t saved;prompter_snapshot(&saved,NULL);
    defaults.brightness=saved.brightness;
#ifndef TP_LIBRARY_HOST_TEST
    if(xTaskCreate(library_playback_worker,"tp_playlist",6144,NULL,1,&playback_worker)!=pdPASS)return ESP_ERR_NO_MEM;
#endif
    return ESP_OK;
}
static cJSON *error(const char *code){cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"ok",false);cJSON_AddStringToObject(j,"error",code);return j;}
static cJSON *success(void){cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"ok",true);cJSON_AddNumberToObject(j,"boot",prompter_boot_id());cJSON_AddNumberToObject(j,"revision",revision);return j;}
static bool select_stored(const char *title)
{
    size_t size;char hash[17],*raw=NULL,*text=load_text(title,&size,&raw,hash);
    prompter_status_t style;
    if(!text||!effective_style(raw,&style)){free(text);free(raw);return false;}
    prompter_control_runtime(STYLE_MASK,&style,false);prompter_set_script(text,size);
    snprintf(active,sizeof(active),"%s",title);++revision;free(text);free(raw);return true;
}
#ifndef TP_LIBRARY_HOST_TEST
static int natural_title(const char *a,const char *b)
{
    const char *original_a=a,*original_b=b;
    while(*a&&*b){
        if(*a>='0'&&*a<='9'&&*b>='0'&&*b<='9'){
            const char *ae=a,*be=b;while(*ae>='0'&&*ae<='9')ae++;while(*be>='0'&&*be<='9')be++;
            while(*a=='0'&&a+1<ae)a++;
            while(*b=='0'&&b+1<be)b++;
            if(ae-a!=be-b)return ae-a<be-b?-1:1;
            int diff=memcmp(a,b,ae-a);if(diff)return diff;a=ae;b=be;continue;
        }
        if((unsigned char)*a!=(unsigned char)*b)return (unsigned char)*a<(unsigned char)*b?-1:1;
        a++;b++;
    }
    int remaining=(*a!=0)-(*b!=0);
    return remaining?remaining:strcmp(original_a,original_b);
}
static double metadata_time(const cJSON *item,const char *key)
{
    const cJSON *value=cJSON_GetObjectItem(item,key);return cJSON_IsNumber(value)?value->valuedouble:0;
}
static int playlist_compare(const char *a,const char *b,int sort,const cJSON *order)
{
    cJSON *am=read_metadata(a),*bm=read_metadata(b);int diff=0;
    if(sort==6){
        const cJSON *ai=cJSON_GetObjectItem(am,"id"),*bi=cJSON_GetObjectItem(bm,"id");
        int ar=cJSON_IsString(ai)?order_rank(order,ai->valuestring):-1,br=cJSON_IsString(bi)?order_rank(order,bi->valuestring):-1;
        if(ar<0)ar=INT_MAX;
        if(br<0)br=INT_MAX;
        diff=(ar>br)-(ar<br);
        if(!diff){double av=metadata_time(am,"createdAt"),bv=metadata_time(bm,"createdAt");diff=(av>bv)-(av<bv);}
    }else if(sort>=2){
        const char *key=sort<4?"updatedAt":"createdAt";
        double av=metadata_time(am,key),bv=metadata_time(bm,key);
        if((av>0)!=(bv>0))diff=av>0?-1:1;
        else diff=((av>bv)-(av<bv))*(sort%2?1:-1);
    }
    cJSON_Delete(am);cJSON_Delete(bm);
    if(!diff)diff=natural_title(a,b)*(sort==1?-1:1);
    return diff;
}
void script_library_advance(uint32_t source_revision)
{
    atomic_store(&advance_revision,source_revision);
    if(playback_worker)xTaskNotifyGive(playback_worker);
}
static void library_playback_worker(void *arg)
{
    (void)arg;
    ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(6000));
    atomic_store(&startup_stage,1);
    if(!firmware_update_in_progress()&&!factory_reset_pending()&&script_library_storage_lock()==ESP_OK){
        atomic_store(&startup_stage,2);
        cJSON *titles=current_titles();
        bool ready=titles&&prepare_identities(titles);
        /* Mount and index storage without selecting a manuscript. The screen
         * remains on its hotspot QR guide until the controller selects one. */
        cJSON_Delete(titles);script_library_storage_unlock();
        atomic_store(&startup_stage,ready?3:4);
    }else atomic_store(&startup_stage,4);
    uint32_t cached_revision=0;int cached_sort=-1;
    for(;;){
        ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));
        uint32_t source=atomic_exchange(&advance_revision,0);
        if(firmware_update_in_progress()||factory_reset_pending())continue;
        xSemaphoreTakeRecursive(mutex,portMAX_DELAY);
        if(factory_reset_pending()){xSemaphoreGiveRecursive(mutex);continue;}
        prompter_status_t state;prompter_snapshot(&state,NULL);
        if(source&&state.script_revision==source&&state.playback_finished&&state.auto_next&&!state.loop&&active[0]){
            cJSON *titles=current_titles(),*order=read_order();
            int layout,sort;uint32_t view_revision;device_settings_library_view(&layout,&sort,&view_revision);
            const cJSON *item;const char *next=NULL;
            cJSON_ArrayForEach(item,titles){
                if(playlist_compare(item->valuestring,active,sort,order)>0&&(!next||playlist_compare(item->valuestring,next,sort,order)<0))next=item->valuestring;
            }
            if(next&&select_stored(next)){
                prompter_status_t playing={.playing=true};
                prompter_control_runtime(TP_PLAYING,&playing,false);
            }
            cJSON_Delete(titles);cJSON_Delete(order);
        }
        int layout,sort;uint32_t view_revision;device_settings_library_view(&layout,&sort,&view_revision);
        if(cached_revision!=revision||cached_sort!=sort){
            char following[TITLE_MAX+1]={0},preceding[TITLE_MAX+1]={0};
            if(card&&!ejected&&active[0]){
                cJSON *titles=current_titles(),*order=read_order();const cJSON *item;const char *next=NULL,*previous=NULL;
                cJSON_ArrayForEach(item,titles){
                    int comparison=playlist_compare(item->valuestring,active,sort,order);
                    if(comparison>0&&(!next||playlist_compare(item->valuestring,next,sort,order)<0))next=item->valuestring;
                    if(comparison<0&&(!previous||playlist_compare(item->valuestring,previous,sort,order)>0))previous=item->valuestring;
                }
                if(next)snprintf(following,sizeof(following),"%s",next);
                if(previous)snprintf(preceding,sizeof(preceding),"%s",previous);
                cJSON_Delete(titles);cJSON_Delete(order);
            }
            portENTER_CRITICAL(&next_title_lock);memcpy(next_title,following,sizeof(next_title));memcpy(previous_title,preceding,sizeof(previous_title));portEXIT_CRITICAL(&next_title_lock);
            cached_revision=revision;cached_sort=sort;
        }
        xSemaphoreGiveRecursive(mutex);
    }
}
#endif
static cJSON *execute(const cJSON *q)
{
    sync_clock(q);
    const cJSON *op=cJSON_GetObjectItem(q,"op");if(!cJSON_IsString(op))return error("invalid_request");
    const char *action=op->valuestring;
#ifndef TP_LIBRARY_HOST_TEST
    if(!strcmp(action,"sd")){
        const cJSON *value=cJSON_GetObjectItem(q,"action");
        const char *operation=cJSON_IsString(value)?value->valuestring:"status";
        if(strcmp(operation,"status")){
            if(firmware_update_in_progress())return error("firmware_busy");
            if(!strcmp(operation,"mount")){
                ejected=false;mount_attempted=true;mount_error=mount_card();
                if(mount_error!=ESP_OK)return error("no_sd_card");
            }else if(!strcmp(operation,"eject")||!strcmp(operation,"format")){
                bool format=!strcmp(operation,"format");
                if(format&&!cJSON_IsTrue(cJSON_GetObjectItem(q,"confirmed")))return error("confirmation_required");
                if(!card||mount_error!=ESP_OK)return error("no_sd_card");
                atomic_store(&maintenance,true);
                if(firmware_update_in_progress()){atomic_store(&maintenance,false);return error("firmware_busy");}
                prompter_status_t pause={.playing=false};prompter_control_runtime(TP_PLAYING,&pause,false);
                sdmmc_card_t *previous=card;
                esp_err_t result=format?esp_vfs_fat_sdcard_format("/sdcard",card):ESP_OK;
                /* IDF may already release the card when remount after format
                 * fails. Check its registration before any further access. */
                if(ff_diskio_get_pdrv_card(previous)!=0xff){
                    esp_err_t unmounted=esp_vfs_fat_sdcard_unmount("/sdcard",previous);
                    if(!format&&unmounted!=ESP_OK){atomic_store(&maintenance,false);return error("sd_eject_failed");}
                    if(unmounted!=ESP_OK&&result==ESP_OK)result=unmounted;
                }
                card=NULL;ejected=true;mount_error=ESP_ERR_INVALID_STATE;active[0]=0;++revision;
                prompter_set_script("",0);
                if(format){
                    if(result==ESP_OK){ejected=false;mount_error=mount_card();result=mount_error;}
                }
                atomic_store(&maintenance,false);
                if(result!=ESP_OK)return error(format?"sd_format_failed":"sd_eject_failed");
            }else return error("invalid_request");
        }
        cJSON *j=success();bool ready=card&&mount_error==ESP_OK;
        cJSON_AddBoolToObject(j,"mounted",ready);cJSON_AddBoolToObject(j,"ejected",ejected);
        cJSON_AddStringToObject(j,"state",ejected?"ejected":ready?"ready":"unavailable");
        cJSON_AddStringToObject(j,"storageError",esp_err_to_name(mount_error));
        if(sd_operation_error[0])cJSON_AddStringToObject(j,"detail",sd_operation_error);
        uint64_t total=0,free_bytes=0;
        if(ready&&esp_vfs_fat_info("/sdcard",&total,&free_bytes)==ESP_OK){
            cJSON_AddNumberToObject(j,"sdTotalBytes",(double)total);cJSON_AddNumberToObject(j,"sdFreeBytes",(double)free_bytes);
            cJSON_AddNumberToObject(j,"sdUsedBytes",(double)(total-free_bytes));
        }
        return j;
    }
    if(!strcmp(action,"deviceSettings")){
        char text[2048];
        if(device_settings_json(text,sizeof(text))!=ESP_OK)return error("out_of_memory");
        cJSON *settings=cJSON_Parse(text);
        if(!settings)return error("out_of_memory");
        cJSON_AddBoolToObject(settings,"ok",true);return settings;
    }
    if(!strcmp(action,"presence")){
        const cJSON *client=cJSON_GetObjectItem(q,"client"),*reading=cJSON_GetObjectItem(q,"reading"),*ble=cJSON_GetObjectItem(q,"bluetooth");
        if(!cJSON_IsString(client)||!cJSON_IsBool(reading)||!cJSON_IsBool(ble))return error("invalid_request");
        return device_settings_presence(client->valuestring,cJSON_IsTrue(reading),cJSON_IsTrue(ble))==ESP_OK?success():error("invalid_request");
    }
#endif
    if(!strcmp(action,"retry")||!strcmp(action,"list")){
        if(!ejected&&(!mount_attempted||!strcmp(action,"retry"))){mount_attempted=true;mount_error=mount_card();}
        cJSON *j=success();cJSON_AddBoolToObject(j,"mounted",card&&mount_error==ESP_OK);
        int layout,sort;uint32_t view_revision;
        device_settings_library_view(&layout,&sort,&view_revision);
        cJSON_AddNumberToObject(j,"libraryLayout",layout);cJSON_AddNumberToObject(j,"librarySort",sort);
        cJSON_AddNumberToObject(j,"settingsRevision",view_revision);
        cJSON_AddStringToObject(j,"directory","/teleprompter");cJSON_AddStringToObject(j,"active",active);
        cJSON_AddStringToObject(j,"storageError",esp_err_to_name(mount_error));
        cJSON *items=cJSON_AddArrayToObject(j,"items");
        if(!card||mount_error!=ESP_OK)return j;
        cJSON *titles=current_titles(),*order=read_order(),*ids=NULL;char tag[17];
        if(titles&&prepare_identities(titles))ids=current_ids(titles);
        if(!ids||!order||!order_tag(ids,order,tag)){cJSON_Delete(titles);cJSON_Delete(ids);cJSON_Delete(order);cJSON_Delete(j);return error("storage_error");}
        cJSON_AddStringToObject(j,"orderEtag",tag);cJSON_Delete(titles);cJSON_Delete(ids);
        DIR *dir=opendir(ROOT);if(!dir){cJSON_Delete(order);cJSON_Delete(j);return error("storage_error");}
        int offset=0;const cJSON *off=cJSON_GetObjectItem(q,"offset");if(cJSON_IsNumber(off)&&off->valueint>=0)offset=off->valueint;
        const cJSON *query_value=cJSON_GetObjectItem(q,"query");const char *query=cJSON_IsString(query_value)?query_value->valuestring:"";
        if(strlen(query)>384){closedir(dir);cJSON_Delete(order);cJSON_Delete(j);return error("invalid_request");}
        const bool compact=cJSON_IsTrue(cJSON_GetObjectItem(q,"index"));
        const cJSON *requested=cJSON_GetObjectItem(q,"titles");
        if(requested&&(!cJSON_IsArray(requested)||cJSON_GetArraySize(requested)>24)){
            closedir(dir);cJSON_Delete(order);cJSON_Delete(j);return error("invalid_request");
        }
        int limit=compact?96:48;
        int count=0,total=0;struct dirent *entry;
        while((entry=readdir(dir))){
            size_t n=strlen(entry->d_name);if(n<5||strcasecmp(entry->d_name+n-4,".txt"))continue;
            char name[TITLE_MAX+1];if(n-4>TITLE_MAX)continue;memcpy(name,entry->d_name,n-4);name[n-4]=0;if(!title_valid(name))continue;
            if(requested){
                bool selected=false;const cJSON *wanted;
                cJSON_ArrayForEach(wanted,requested)if(cJSON_IsString(wanted)&&!strcmp(name,wanted->valuestring)){selected=true;break;}
                if(!selected)continue;
            }
            char path[256];path_for(path,name,"txt");struct stat st;if(stat(path,&st)||!S_ISREG(st.st_mode))continue;
            char *body=query[0]?read_file(path,PROMPTER_MAX_BYTES,NULL):NULL;
            if(!search_matches(name,body,query)){free(body);continue;}
            if(total++<offset||count>=limit){free(body);continue;}
            cJSON *item=cJSON_CreateObject();cJSON_AddStringToObject(item,"title",name);cJSON_AddNumberToObject(item,"bytes",st.st_size);
            cJSON *meta=read_metadata(name);const cJSON *id=cJSON_GetObjectItem(meta,"id");
            int rank=order_rank(order,id->valuestring);
            cJSON_AddStringToObject(item,"id",id->valuestring);cJSON_AddNumberToObject(item,"order",rank);
            cJSON_AddBoolToObject(item,"identityIssue",rank<0&&cJSON_IsTrue(cJSON_GetObjectItem(meta,"identityIssue")));cJSON_Delete(meta);
            if(!compact&&!body)body=read_file(path,PROMPTER_MAX_BYTES,NULL);
            add_metadata(item,name,compact?NULL:body);if(!compact)add_excerpt(item,body);free(body);
            path_for(path,name,"json");cJSON_AddBoolToObject(item,"hasStyle",exists(path));cJSON_AddItemToArray(items,item);count++;
        }
        closedir(dir);cJSON_Delete(order);cJSON_AddBoolToObject(j,"compact",compact);cJSON_AddNumberToObject(j,"total",total);cJSON_AddNumberToObject(j,"next",offset+count<total?offset+count:-1);return j;
    }
    if(!strcmp(action,"defaults")){cJSON *j=success();cJSON_AddItemToObject(j,"style",style_json(&defaults));return j;}
#ifndef TP_LIBRARY_HOST_TEST
    if(!strcmp(action,"storage")){
        cJSON *j=success();uint64_t total=0,available=0;
        if(card&&esp_vfs_fat_info("/sdcard",&total,&available)==ESP_OK){
            cJSON_AddNumberToObject(j,"sdTotalBytes",(double)total);cJSON_AddNumberToObject(j,"sdFreeBytes",(double)available);
        }
        const esp_partition_t *app=esp_ota_get_running_partition();
        if(app)cJSON_AddNumberToObject(j,"appSlotBytes",app->size);
        nvs_stats_t stats;if(nvs_get_stats(NULL,&stats)==ESP_OK){
            cJSON_AddNumberToObject(j,"nvsUsedEntries",stats.used_entries);cJSON_AddNumberToObject(j,"nvsFreeEntries",stats.free_entries);
        }
        cJSON_AddNumberToObject(j,"internalFreeBytes",heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        cJSON_AddNumberToObject(j,"psramFreeBytes",heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        struct stat st;if(!stat(ROOT"/.order.json",&st))cJSON_AddNumberToObject(j,"orderBytes",st.st_size);else cJSON_AddNumberToObject(j,"orderBytes",0);
        return j;
    }
#endif
    if(!card||mount_error!=ESP_OK)return error("no_sd_card");
    if(!strcmp(action,"reorder")){
        const cJSON *requested=cJSON_GetObjectItem(q,"ids"),*expected=cJSON_GetObjectItem(q,"expected");
        if(!cJSON_IsArray(requested)||!cJSON_IsString(expected))return error("invalid_request");
        cJSON *names=current_titles(),*titles=names&&prepare_identities(names)?current_ids(names):NULL,*order=read_order();char tag[17];cJSON_Delete(names);
        if(!titles||!order||!order_tag(titles,order,tag)){cJSON_Delete(titles);cJSON_Delete(order);return error("storage_error");}
        bool conflict=strcmp(expected->valuestring,tag)!=0;
        if(conflict){cJSON_Delete(titles);cJSON_Delete(order);return error("order_conflict");}
        bool valid=cJSON_GetArraySize(requested)==cJSON_GetArraySize(titles);const cJSON *item;
        cJSON_ArrayForEach(item,requested){
            if(!cJSON_IsString(item)||!identity_valid(item->valuestring)||order_rank(titles,item->valuestring)<0){valid=false;break;}
            int occurrences=0;const cJSON *other;cJSON_ArrayForEach(other,requested)if(cJSON_IsString(other)&&!strcmp(item->valuestring,other->valuestring))occurrences++;
            if(occurrences!=1){valid=false;break;}
        }
        if(!valid){cJSON_Delete(titles);cJSON_Delete(order);return error("invalid_order");}
        /* Keep absent identities so ambiguous external renames do not erase
         * the only remaining record of their original position. */
        cJSON *merged=cJSON_Duplicate(order,true);int next=0;
        if(!merged){cJSON_Delete(titles);cJSON_Delete(order);return error("storage_error");}
        for(int i=0;i<cJSON_GetArraySize(merged);i++){
            const cJSON *old=cJSON_GetArrayItem(merged,i);
            if(cJSON_IsString(old)&&order_rank(titles,old->valuestring)>=0)
                cJSON_ReplaceItemInArray(merged,i,cJSON_Duplicate(cJSON_GetArrayItem(requested,next++),true));
        }
        while(next<cJSON_GetArraySize(requested))cJSON_AddItemToArray(merged,cJSON_Duplicate(cJSON_GetArrayItem(requested,next++),true));
        cJSON_Delete(titles);cJSON_Delete(order);
        bool saved=merged&&write_order(merged);cJSON_Delete(merged);
        return saved?success():error("storage_error");
    }
    const cJSON *name=cJSON_GetObjectItem(q,"title");if(!cJSON_IsString(name)||!title_valid(name->valuestring))return error("invalid_title");
    const char *title=name->valuestring;char hash[17]={0};size_t size=0;char *raw=NULL,*text=load_text(title,&size,&raw,hash);
    bool is_new=!strcmp(action,"create");cJSON *result=NULL;
    if(!text&&!is_new){result=error("missing_or_invalid_script");goto done;}
    const cJSON *expected=cJSON_GetObjectItem(q,"expected");
    if(strcmp(action,"get")&&strcmp(action,"select")&&!is_new&&(!cJSON_IsString(expected)||strcmp(expected->valuestring,hash))){result=error("conflict");goto done;}
    prompter_status_t style;bool style_valid=effective_style(raw,&style);
    if(!strcmp(action,"get")){
        result=success();cJSON_AddStringToObject(result,"title",title);cJSON_AddStringToObject(result,"text",text);
        cJSON_AddStringToObject(result,"etag",hash);cJSON_AddBoolToObject(result,"hasStyle",raw!=NULL);cJSON_AddBoolToObject(result,"styleValid",style_valid);
        add_metadata(result,title,text);
        cJSON_AddItemToObject(result,"style",style_json(&style));goto done;
    }
    if(!strcmp(action,"select")){
        if(!style_valid){result=error("invalid_style");goto done;}
        if(!select_stored(title)){result=error("storage_error");goto done;}
        result=success();cJSON_AddStringToObject(result,"active",active);device_settings_activity();goto done;
    }
    if(!strcmp(action,"delete")){
        if(!transaction(title,NULL,NULL,true)){result=error("storage_error");goto done;}
        if(!strcmp(title,active)){
            active[0]=0;prompter_set_script("",0);prompter_control_runtime(STYLE_MASK,&defaults,false);
        }
        result=success();goto done;
    }
    if(!strcmp(action,"save")||is_new){
        char path[256];path_for(path,title,"txt");if(is_new&&exists(path)){result=error("already_exists");goto done;}
        const cJSON *body=cJSON_GetObjectItem(q,"text"),*patch=cJSON_GetObjectItem(q,"style");
        if(!cJSON_IsString(body)||!prompter_text_valid(body->valuestring,strlen(body->valuestring))){result=error("invalid_text");goto done;}
        char *next_style=NULL;
        if(patch&&!cJSON_IsNull(patch)){
            style=defaults;unsigned mask;if(!parse_style(patch,&style,&mask)){result=error("invalid_style");goto done;}
            next_style=style_document(&style);if(!next_style){result=error("out_of_memory");goto done;}
        }else if(!patch&&raw)next_style=strdup(raw);
        if(!transaction(title,body->valuestring,next_style,false)){free(next_style);result=error("storage_error");goto done;}
        file_revision(body->valuestring,strlen(body->valuestring),next_style,hash);
        if(!strcmp(active,title)){
            effective_style(next_style,&style);prompter_control_runtime(STYLE_MASK,&style,false);prompter_set_script(body->valuestring,strlen(body->valuestring));
        }
        free(next_style);result=success();cJSON_AddStringToObject(result,"etag",hash);device_settings_activity();goto done;
    }
    result=error("unknown_operation");
done:free(text);free(raw);return result;
}
#ifndef TP_LIBRARY_HOST_TEST
static void format_worker(void *unused)
{
    (void)unused;
    cJSON *q=cJSON_CreateObject();cJSON_AddStringToObject(q,"op","sd");cJSON_AddStringToObject(q,"action","format");cJSON_AddBoolToObject(q,"confirmed",true);
    xSemaphoreTakeRecursive(mutex,portMAX_DELAY);
    cJSON *result=execute(q);const cJSON *error_value=cJSON_GetObjectItem(result,"error");
    snprintf(sd_operation_error,sizeof(sd_operation_error),"%s",cJSON_IsString(error_value)?error_value->valuestring:"");
    cJSON_Delete(result);cJSON_Delete(q);atomic_store(&maintenance,false);
    xSemaphoreGiveRecursive(mutex);vTaskDelete(NULL);
}
#endif
char *script_library_request(const char *request, tp_request_source_t source)
{
#ifdef TP_LIBRARY_HOST_TEST
    (void)source;
#endif
    cJSON *q=cJSON_ParseWithOpts(request,NULL,true),*result;
#ifndef TP_LIBRARY_HOST_TEST
    const cJSON *operation=cJSON_GetObjectItem(q,"op");
    if(factory_reset_pending()&&!(cJSON_IsString(operation)&&!strcmp(operation->valuestring,"firmware"))){
        result=error("factory_reset_busy");char *out=cJSON_PrintUnformatted(result);cJSON_Delete(result);cJSON_Delete(q);return out;
    }
    if(cJSON_IsString(operation)&&!strcmp(operation->valuestring,"sd")){
        const cJSON *action=cJSON_GetObjectItem(q,"action");
        bool format=cJSON_IsString(action)&&!strcmp(action->valuestring,"format");
        if(atomic_load(&maintenance)){
            result=success();cJSON_AddStringToObject(result,"state","formatting");cJSON_AddBoolToObject(result,"mounted",false);
            char *out=cJSON_PrintUnformatted(result);cJSON_Delete(result);cJSON_Delete(q);return out;
        }
        if(format){
            xSemaphoreTakeRecursive(mutex,portMAX_DELAY);
            if(!cJSON_IsTrue(cJSON_GetObjectItem(q,"confirmed")))result=error("confirmation_required");
            else if(firmware_update_in_progress())result=error("firmware_busy");
            else if(!card||mount_error!=ESP_OK)result=error("no_sd_card");
            else {
                sd_operation_error[0]=0;atomic_store(&maintenance,true);
                if(xTaskCreate(format_worker,"sd_format",8192,NULL,1,NULL)!=pdPASS){atomic_store(&maintenance,false);result=error("out_of_memory");}
                else {result=success();cJSON_AddStringToObject(result,"state","formatting");cJSON_AddBoolToObject(result,"mounted",false);}
            }
            xSemaphoreGiveRecursive(mutex);
            char *out=cJSON_PrintUnformatted(result);cJSON_Delete(result);cJSON_Delete(q);return out;
        }
    }
    if(atomic_load(&maintenance)&&!(cJSON_IsString(operation)&&!strcmp(operation->valuestring,"firmware"))){
        result=error("sd_busy");char *out=cJSON_PrintUnformatted(result);cJSON_Delete(result);cJSON_Delete(q);return out;
    }
    if(cJSON_IsString(operation)&&!strcmp(operation->valuestring,"firmware")){
        result=firmware_update_request(q);char *json=cJSON_PrintUnformatted(result);
        cJSON_Delete(result);cJSON_Delete(q);return json;
    }
    if(firmware_update_in_progress()){
        result=error("firmware_busy");char *json=cJSON_PrintUnformatted(result);
        cJSON_Delete(result);cJSON_Delete(q);return json;
    }
    if(cJSON_IsString(operation)&&!strcmp(operation->valuestring,"wifi")){
        result=wifi_settings_request(q,source);char *json=result?cJSON_PrintUnformatted(result):NULL;
        cJSON_Delete(result);cJSON_Delete(q);return json;
    }
    if(cJSON_IsString(operation)&&!strcmp(operation->valuestring,"preview")){
        cJSON_Delete(q);return teleprompter_preview_snapshot();
    }
    if(cJSON_IsString(operation)&&!strcmp(operation->valuestring,"telemetry")){
        cJSON_Delete(q);return teleprompter_telemetry_snapshot();
    }
#endif
    xSemaphoreTakeRecursive(mutex,portMAX_DELAY);
#ifndef TP_LIBRARY_HOST_TEST
    result=factory_reset_pending()?error("factory_reset_busy"):
        cJSON_IsObject(q)?execute(q):error("invalid_request");
#else
    result=cJSON_IsObject(q)?execute(q):error("invalid_request");
#endif
    char *out=cJSON_PrintUnformatted(result);cJSON_Delete(result);cJSON_Delete(q);
    xSemaphoreGiveRecursive(mutex);return out;
}
esp_err_t script_library_control(unsigned mask,const prompter_status_t *v)
{
    if(!v)return ESP_ERR_INVALID_ARG;
    prompter_status_t normalized=*v;
    prompter_normalize_playback(&mask,&normalized);v=&normalized;
#ifndef TP_LIBRARY_HOST_TEST
    if(firmware_update_in_progress()||factory_reset_pending())return ESP_ERR_INVALID_STATE;
#endif
    if(!mutex)return prompter_control_runtime(mask,v,true);
    esp_err_t err=prompter_control_validate(mask,v);if(err!=ESP_OK)return err;
    xSemaphoreTakeRecursive(mutex,portMAX_DELAY);
#ifndef TP_LIBRARY_HOST_TEST
    if(factory_reset_pending()){xSemaphoreGiveRecursive(mutex);return ESP_ERR_INVALID_STATE;}
#endif
    if(active[0]){
        if(!card||mount_error!=ESP_OK){err=ESP_ERR_INVALID_STATE;goto done;}
        if(!(mask&STYLE_MASK)){
            if(mask&(TP_ROTATION|TP_BRIGHTNESS)){
                prompter_status_t next=defaults;
                if(mask&TP_BRIGHTNESS)next.brightness=v->brightness;
                err=prompter_store_defaults(&next);if(err!=ESP_OK)goto done;defaults=next;
            }
            err=prompter_control_runtime(mask,v,false);goto done;
        }
        char hash[17],*raw=NULL;size_t n;char *text=load_text(active,&n,&raw,hash);prompter_status_t style;
        if(!text||!effective_style(raw,&style)){free(text);free(raw);err=ESP_FAIL;goto done;}
        for(size_t i=0;i<sizeof(fields)/sizeof(fields[0]);i++)if(mask&fields[i].mask){
            size_t count=fields[i].boolean?sizeof(bool):sizeof(int);
            memcpy((char*)&style+fields[i].offset,(const char*)v+fields[i].offset,count);
        }
        char *next=style_document(&style);bool ok=next&&transaction(active,text,next,false);
        free(next);free(raw);free(text);if(!ok){err=ESP_FAIL;goto done;}
        err=prompter_control_runtime(mask,v,false);
        if(err==ESP_OK&&(mask&(TP_ROTATION|TP_BRIGHTNESS))){
            defaults.brightness=(mask&TP_BRIGHTNESS)?v->brightness:defaults.brightness;
            /* Brightness is shared. Orientation belongs to the manuscript. */
            err=prompter_store_defaults(&defaults);
        }
    }else{
        err=prompter_control_runtime(mask,v,true);
        if(err==ESP_OK){
            /* Editing an unselected screen must not redefine the product style. */
            prompter_status_t saved;prompter_snapshot(&saved,NULL);
            defaults.brightness=saved.brightness;
        }
    }
done:xSemaphoreGiveRecursive(mutex);return err;
}
