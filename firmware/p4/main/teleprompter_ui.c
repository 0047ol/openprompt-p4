#include "teleprompter_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "esp_log.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "display_port.h"
#include "guide_clip.h"
#include "prompter_state.h"
#include "wifi_settings.h"
#include "control_session.h"
#include "connection_guide_copy.h"
#include "ui_motion.h"
#include "device_settings.h"
#include "script_library.h"
#include "cJSON.h"
#include "freertos/semphr.h"
#include "src/misc/lv_text_private.h"

extern const uint8_t font_start[] asm("_binary_noto_sc_subset_otf_start");
extern const uint8_t font_end[] asm("_binary_noto_sc_subset_otf_end");
static lv_font_t *s_font, *s_font_source;
static lv_font_t s_tight_font;
static bool tight_descriptor(const lv_font_t *font,lv_font_glyph_dsc_t *out,uint32_t cp,uint32_t next)
{
    (void)font;(void)next;
    if(!s_font_source->get_glyph_dsc(s_font_source,out,cp,0))return false;
    if(out->box_w){out->adv_w=out->box_w;out->ofs_x=0;}
    return true;
}
static const void *tight_bitmap(lv_font_glyph_dsc_t *glyph,lv_draw_buf_t *buffer)
{
    /* Keep TinyTTF's original cache key and font scale. */
    const lv_font_t *saved=glyph->resolved_font;glyph->resolved_font=s_font_source;
    const void *result=s_font_source->get_glyph_bitmap(glyph,buffer);
    glyph->resolved_font=saved;return result;
}
static void tight_release(const lv_font_t *font,lv_font_glyph_dsc_t *glyph)
{(void)font;s_font_source->release_glyph(s_font_source,glyph);}
static void tight_metrics(int size,const char *text)
{
    lv_tiny_ttf_set_size(s_font_source,size);
    s_tight_font=*s_font_source;s_tight_font.get_glyph_dsc=tight_descriptor;
    s_tight_font.get_glyph_bitmap=tight_bitmap;s_tight_font.release_glyph=tight_release;
    int top=INT32_MAX,bottom=INT32_MIN;
    for(uint32_t offset=0;text[offset];){
        uint32_t cp,next;lv_text_encoded_letter_next_2(text,&cp,&next,&offset);
        lv_font_glyph_dsc_t glyph={0};
        if(!s_font_source->get_glyph_dsc(s_font_source,&glyph,cp,0)||!glyph.box_h)continue;
        int a=-(int)glyph.box_h-glyph.ofs_y,b=-glyph.ofs_y;
        if(a<top)top=a;
        if(b>bottom)bottom=b;
    }
    if(top!=INT32_MAX){s_tight_font.line_height=bottom-top;s_tight_font.base_line=bottom;}
}
static lv_font_t *s_countdown_font;
static lv_font_t *s_info_font, *s_ready_font, *s_bar_font, *s_guide_font, *s_step_font;
static int s_footer_font_size=22;
static int s_language=-1;
static bool s_show_info;
static device_bars_t s_bars;
static lv_obj_t *s_header_left,*s_header_right,*s_footer_right,*s_timecode;
static lv_obj_t *s_header_separators[32],*s_footer_separators[32];
static lv_obj_t *s_footer_values[TP_BAR_FOOTER_MAX];
static int s_footer_icon_size=16;
static lv_obj_t *s_state_icon,*s_play_dot,*s_pause_icon[2],*s_stop_icon;
static bool s_play_dot_active,s_play_dot_bright;
static uint32_t s_play_dot_started;
static uint32_t s_bar_tick;
static bool s_bar_update_pending;
static int s_viewport_top=100;
static double s_fps,s_jitter_ms,s_frame_ms,s_render_ms;
static uint32_t s_metric_frames;
static uint64_t s_drawn_frames,s_late_frames;
static int64_t s_metric_start,s_metric_last;
static double s_metric_sum,s_metric_sum2,s_metric_render;
static char s_telemetry[4096]="{\"ok\":true}";
static int s_countdown_digit=-1;
static uint32_t s_digit_started;
static int s_countdown_shown = -1;
#define COUNTDOWN_SIZE 320
static lv_obj_t *s_countdown_canvas;
static lv_draw_buf_t *s_countdown_buffer;
static uint16_t *s_countdown_pixels;
static uint32_t s_countdown_frame;
static int s_countdown_font_size;

static const char *TAG = "tp_ui";
static lv_obj_t *s_script_label;
static lv_obj_t *s_footer;
static lv_obj_t *s_viewport;
static lv_obj_t *s_title;
static lv_obj_t *s_welcome_card, *s_welcome_title, *s_welcome_body, *s_welcome_qr;
static bool s_guide_only,s_guide_changed;
void teleprompter_ui_show_guide(bool visible){if(s_guide_only!=visible){s_guide_only=visible;s_guide_changed=true;}}
static lv_obj_t *s_welcome_steps, *s_welcome_content, *s_welcome_visual, *s_welcome_copy;
static lv_obj_t *s_welcome_title_row, *s_welcome_loader, *s_welcome_description, *s_welcome_body_more;
static char s_welcome_body_text[1024];
static lv_obj_t *s_step_dot[TP_GUIDE_STEP_COUNT], *s_step_number[TP_GUIDE_STEP_COUNT], *s_step_label[TP_GUIDE_STEP_COUNT];
static tp_guide_tracker_t s_guide_tracker;
static lv_obj_t *s_reading_marker;
static bool s_layout_ready;
static bool s_reader_cache_pending,s_guide_animation_pending,s_ui_presentable;
static bool s_guide_sliding;
static uint32_t s_guide_slide_started,s_guide_slide_frames;
static int64_t s_guide_slide_last_us;
static uint32_t s_guide_slide_max_gap_us,s_guide_slide_max_present_us;
static int s_guide_slide_offset;
void teleprompter_ui_resume_motions(void)
{
    s_ui_presentable=true;
    s_guide_animation_pending=true;
}
static prompter_status_t s_previous;
static char s_text[PROMPTER_MAX_BYTES + 1];
static float s_scroll;
static uint32_t s_tick;
static char s_address[16];
static char s_welcome[384];
static char s_welcome_qr_payload[256];

#define TILE_HEIGHT 256
#define TILE_COUNT 24
#define TILE_BASE_COUNT 12
/* A rotated destination cache line contains 32 RGB565 pixels. */
#define TILE_PREPARE_ROWS 32
#define TILE_MEMORY_RESERVE (4 * 1024 * 1024)
#define VIEWPORT_TOP s_viewport_top
typedef struct { int index; bool valid; uint16_t *pixels, *dim_pixels; } script_tile_t;
static script_tile_t s_tiles[TILE_COUNT];
static unsigned s_tile_count=TILE_BASE_COUNT;
static bool s_cache_all;
static script_tile_t s_empty_tile;
static int s_preparing_index=INT32_MIN,s_preparing_y;
static lv_obj_t *s_canvas;
static lv_draw_buf_t *s_canvas_buffer;
static int s_width, s_height, s_text_height, s_rotation, s_letter_spacing, s_line_spacing;
static int s_text_origin;
static int s_margin_left, s_text_align, s_mirror, s_band_top, s_band_bottom;
static uint32_t s_text_color;
static bool s_center_highlight, s_sight_guide, s_bold;
static uint16_t *s_guide_pixels;
static uint16_t *s_blank_tile;
static int s_last_y = INT32_MIN;
static bool s_force_present;
static unsigned s_guide_repaint=2;
static uint64_t s_compose_us, s_build_us;
static uint32_t s_compose_frames, s_built_tiles, s_max_compose_us;
static uint32_t s_visible_tile_misses,s_max_prepare_us;
static int64_t s_report_us;
static SemaphoreHandle_t s_preview_mutex;
static char *s_preview_json;
static uint32_t s_preview_revision;
/* Reuse exact wrapped row boundaries instead of measuring the preceding
 * portion of the whole script again for every newly prepared tile. */
static uint32_t s_row_offsets[PROMPTER_MAX_BYTES + 2];
static unsigned s_row_count;
static unsigned s_playback_rows;
static int s_loop_period,s_end_scroll,s_repeat_tile_span;
static int s_frame_tiles[64];
static unsigned s_frame_tile_count,s_evict_slot;
static script_tile_t *s_preparing_tile;

esp_err_t teleprompter_preview_init(void)
{
    s_preview_mutex=xSemaphoreCreateMutex();
    return s_preview_mutex?ESP_OK:ESP_ERR_NO_MEM;
}

char *teleprompter_preview_snapshot(void)
{
    if(!s_preview_mutex)return strdup("{\"ok\":false,\"error\":\"preview_not_ready\"}");
    xSemaphoreTake(s_preview_mutex,portMAX_DELAY);
    char *copy=strdup(s_preview_json?s_preview_json:"{\"ok\":false,\"error\":\"preview_not_ready\"}");
    xSemaphoreGive(s_preview_mutex);
    return copy;
}

char *teleprompter_telemetry_snapshot(void)
{
    if(!s_preview_mutex)return strdup("{\"ok\":false,\"error\":\"preview_not_ready\"}");
    xSemaphoreTake(s_preview_mutex,portMAX_DELAY);char *copy=strdup(s_telemetry);xSemaphoreGive(s_preview_mutex);return copy;
}

/* Centre adornments on visible glyph ink, not TinyTTF's larger line box. */
static void bar_ink(const lv_font_t *font,const char *text,int *top,int *bottom)
{
    *top=INT32_MAX;*bottom=INT32_MIN;
    for(uint32_t i=0;text[i];){
        uint32_t cp,next;lv_text_encoded_letter_next_2(text,&cp,&next,&i);
        lv_font_glyph_dsc_t glyph={0};
        if(!font->get_glyph_dsc(font,&glyph,cp,0)||!glyph.box_h)continue;
        int a=font->line_height-font->base_line-glyph.box_h-glyph.ofs_y;
        int b=a+glyph.box_h;
        if(a<*top)*top=a;
        if(b>*bottom)*bottom=b;
    }
    if(*top==INT32_MAX){*top=0;*bottom=font->line_height;}
}

static void bar_visible(char *out,size_t capacity,const char *text)
{
    snprintf(out,capacity,"%s",text);
    for(char *p=out;*p;++p){
        if(*p=='|')*p=' ';
        if(!strncmp(p,"{sun}",5)||!strncmp(p,"{clk}",5)){memset(p,' ',5);p+=4;}
    }
}

static int bar_width(const char *text,const lv_font_t *font)
{
    char visible[1536];bar_visible(visible,sizeof(visible),text);
    lv_text_attributes_t attributes={0};
    return lv_text_get_width(visible,strlen(visible),font,&attributes);
}

/* Circular sun and clock, matching screen-bars.js; no font symbol fallback. */
static const int8_t s_bar_paths[2][16][4]={
    {{10,1,10,3},{10,17,10,19},{1,10,3,10},{17,10,19,10},
     {4,4,5,5},{15,15,16,16},{4,16,5,15},{15,5,16,4}},
    {{10,5,10,10},{10,10,14,12}}
};
static const unsigned s_bar_path_counts[]={8,2};
static lv_obj_t *s_bar_lines[2][16];
static lv_obj_t *s_bar_circles[2];
static lv_point_precise_t s_bar_points[2][16][2];
static int s_bar_path_size[2];

static int bar_icon_size(void){return s_footer_icon_size;}
static int footer_field_width(const char *field)
{
    bool icon=!strncmp(field,"{sun}",5)||!strncmp(field,"{clk}",5);
    return bar_width(icon?field+6:field,s_bar_font)+(icon?bar_icon_size()+6:0);
}

static void footer_text(const char *text)
{
    /* LVGL's style setters invalidate even when their value is unchanged.
     * Telemetry ticks must not redraw all labels and icon rays every second.
     * TinyTTF mutates an existing font, so pointer equality alone is unsafe. */
    static char previous_text[1536];
    static lv_obj_t *previous_owner;
    static const lv_font_t *previous_font;
    static int previous_width=-1,previous_height=-1,previous_size=-1,previous_line=-1,previous_icon=-1;
    static int label_size[TP_BAR_FOOTER_MAX],label_line[TP_BAR_FOOTER_MAX];
    int available=lv_obj_get_width(s_footer_right),icon_size=bar_icon_size();
    bool font_changed=previous_font!=s_bar_font||previous_size!=s_bars.footer_font_size||
                      previous_line!=s_bar_font->line_height;
    if(previous_owner==s_footer_right&&!font_changed&&previous_width==available&&
       previous_height==s_bars.footer_height&&previous_icon==icon_size&&!strcmp(previous_text,text))return;
    char fields[TP_BAR_FOOTER_MAX][128];unsigned count=0;int width=0;
    int field_widths[TP_BAR_FOOTER_MAX];
    for(const char *part=text;*part&&count<TP_BAR_FOOTER_MAX;){
        const char *next=strstr(part," | ");size_t length=next?(size_t)(next-part):strlen(part);
        snprintf(fields[count],sizeof(fields[count]),"%.*s",(int)length,part);
        field_widths[count]=footer_field_width(fields[count]);
        width+=field_widths[count];if(count)width+=24;
        ++count;if(!next)break;part=next+3;
    }
    int x=lv_obj_get_width(s_footer_right)-width,center=s_bars.footer_height/2;
    bool icons[2]={false,false};
    for(unsigned i=0;i<TP_BAR_FOOTER_MAX;++i){
        if(!s_footer_values[i]){
            s_footer_values[i]=lv_label_create(s_footer_right);
            lv_obj_remove_style_all(s_footer_values[i]);
            lv_obj_set_style_text_color(s_footer_values[i],lv_color_hex(0x8aa4b8),0);
            lv_obj_set_scrollable(s_footer_values[i],false);
            lv_label_set_long_mode(s_footer_values[i],LV_LABEL_LONG_MODE_CLIP);
        }
        lv_obj_set_hidden(s_footer_values[i],i>=count);
        if(i>=count)continue;
        if(i){
            if(!s_footer_separators[i]){
                s_footer_separators[i]=lv_obj_create(s_footer_right);lv_obj_remove_style_all(s_footer_separators[i]);
                lv_obj_set_style_bg_color(s_footer_separators[i],lv_color_hex(0x536572),0);
                lv_obj_set_style_bg_opa(s_footer_separators[i],LV_OPA_COVER,0);
                lv_obj_set_scrollable(s_footer_separators[i],false);
            }
            int h=s_footer_font_size/2;if(h<6)h=6;
            lv_obj_set_size(s_footer_separators[i],1,h);
            lv_obj_set_pos(s_footer_separators[i],x+12,center-h/2);
            lv_obj_set_hidden(s_footer_separators[i],false);x+=24;
        }
        const char *value=fields[i];bool icon=!strncmp(value,"{sun}",5)||!strncmp(value,"{clk}",5);
        if(icon){
            unsigned type=value[1]=='s'?0:1;icons[type]=true;int size=bar_icon_size(),y=center-size/2;
            if(!s_bar_circles[type]){
                s_bar_circles[type]=lv_obj_create(s_footer_right);lv_obj_remove_style_all(s_bar_circles[type]);
                lv_obj_set_style_radius(s_bar_circles[type],LV_RADIUS_CIRCLE,0);
                lv_obj_set_style_border_color(s_bar_circles[type],lv_color_hex(0x8aa4b8),0);
                lv_obj_set_scrollable(s_bar_circles[type],false);
            }
            int inset=type?1:size/3;
            lv_obj_set_size(s_bar_circles[type],size-2*inset,size-2*inset);
            lv_obj_set_pos(s_bar_circles[type],x+inset,y+inset);
            if(s_bar_path_size[type]!=size)lv_obj_set_style_border_width(s_bar_circles[type],size>=16?2:1,0);
            lv_obj_set_hidden(s_bar_circles[type],false);
            for(unsigned j=0;j<s_bar_path_counts[type];++j){
                if(!s_bar_lines[type][j]){
                    s_bar_lines[type][j]=lv_line_create(s_footer_right);lv_obj_remove_style_all(s_bar_lines[type][j]);
                    lv_obj_set_style_line_color(s_bar_lines[type][j],lv_color_hex(0x8aa4b8),0);
                    lv_obj_set_style_line_rounded(s_bar_lines[type][j],true,0);
                }
                /* Unchanged points still invalidate the LVGL line. Avoid
                 * redrawing every ray on each telemetry-only update. */
                if(s_bar_path_size[type]!=size){
                    lv_obj_set_style_line_width(s_bar_lines[type][j],size>=16?2:1,0);
                    for(unsigned k=0;k<2;++k)s_bar_points[type][j][k]=(lv_point_precise_t){
                        s_bar_paths[type][j][k*2]*size/20,s_bar_paths[type][j][k*2+1]*size/20};
                    lv_line_set_points(s_bar_lines[type][j],s_bar_points[type][j],2);
                }
                lv_obj_set_pos(s_bar_lines[type][j],x,y);lv_obj_set_hidden(s_bar_lines[type][j],false);
            }
            s_bar_path_size[type]=size;
            x+=size+6;value+=6;
        }
        int text_width=field_widths[i]-(icon?bar_icon_size()+6:0);
        if(font_changed||label_size[i]!=s_bars.footer_font_size||label_line[i]!=s_bar_font->line_height||
           lv_obj_get_style_text_font(s_footer_values[i],LV_PART_MAIN)!=s_bar_font)
            lv_obj_set_style_text_font(s_footer_values[i],s_bar_font,0);
        label_size[i]=s_bars.footer_font_size;label_line[i]=s_bar_font->line_height;
        lv_obj_set_size(s_footer_values[i],text_width,s_bar_font->line_height);
        if(strcmp(lv_label_get_text(s_footer_values[i]),value))lv_label_set_text(s_footer_values[i],value);
        int top,bottom;bar_ink(s_bar_font,value,&top,&bottom);
        lv_obj_set_pos(s_footer_values[i],x,center-(top+bottom)/2);
        x+=text_width;
    }
    for(unsigned i=count?count:1;i<TP_BAR_FOOTER_MAX;++i)if(s_footer_separators[i])lv_obj_set_hidden(s_footer_separators[i],true);
    for(unsigned type=0;type<2;++type)if(!icons[type]){
        if(s_bar_circles[type])lv_obj_set_hidden(s_bar_circles[type],true);
        for(unsigned j=0;j<s_bar_path_counts[type];++j)if(s_bar_lines[type][j])lv_obj_set_hidden(s_bar_lines[type][j],true);
    }
    snprintf(previous_text,sizeof(previous_text),"%s",text);
    previous_owner=s_footer_right;previous_font=s_bar_font;previous_width=available;
    previous_height=s_bars.footer_height;previous_size=s_bars.footer_font_size;
    previous_line=s_bar_font->line_height;previous_icon=icon_size;
}

static void bar_text(lv_obj_t *label,const char *text)
{
    if(label==s_footer_right){footer_text(text);return;}
    lv_obj_t **separators=label==s_header_right?s_header_separators:NULL;
    if(!separators){if(strcmp(lv_label_get_text(label),text))lv_label_set_text(label,text);return;}
    char visible[1536];bar_visible(visible,sizeof(visible),text);
    bool changed=strcmp(lv_label_get_text(label),visible)!=0;
    static int header_width=-1,header_size=-1,header_line_height=-1;
    static char header_text[1536];
    int width=lv_obj_get_width(label);
    if(!changed&&!strcmp(header_text,text)&&header_width==width&&header_size==s_bars.header_font_size&&header_line_height==s_info_font->line_height)return;
    snprintf(header_text,sizeof(header_text),"%s",text);
    header_width=width;header_size=s_bars.header_font_size;header_line_height=s_info_font->line_height;
    if(changed)lv_label_set_text(label,visible);
    const lv_font_t *font=label==s_header_right?s_info_font:s_bar_font;
    int top,bottom;bar_ink(font,visible,&top,&bottom);
    int height=(bottom-top)/2,center=(top+bottom)/2;
    lv_text_attributes_t attributes={0};unsigned used=0;
    int origin=lv_obj_get_width(label)-bar_width(text,font);
    for(size_t i=0;text[i];++i){
        if(text[i]=='|'&&used<32){
            /* Prefix measurement belongs to an actual separator. Doing it
             * for every UTF-8 byte made the periodic header update quadratic. */
            int x=origin+lv_text_get_width(visible,i,font,&attributes);
            if(!separators[used]){
                separators[used]=lv_obj_create(label);lv_obj_remove_style_all(separators[used]);
                lv_obj_set_style_bg_color(separators[used],lv_color_hex(0x536572),0);
                lv_obj_set_style_bg_opa(separators[used],LV_OPA_COVER,0);
                lv_obj_set_clickable(separators[used],false);lv_obj_set_scrollable(separators[used],false);
            }
            lv_obj_set_size(separators[used],1,height);
            lv_obj_set_pos(separators[used],x+lv_text_get_width(" ",1,font,&attributes)/2,center-height/2);
            lv_obj_set_hidden(separators[used++],false);
        }
    }
    for(;used<32;++used)if(separators[used])lv_obj_set_hidden(separators[used],true);
}

/* Keep complete fields on ONE line, at most six. Never wrap or clip a field. */
static unsigned bar_fit(char *text,size_t capacity,int width,const lv_font_t *font,unsigned *total)
{
    char fields[24][128];*total=0;
    for(char *part=text;part&&*part&&*total<24;){
        char *next=strstr(part," | ");if(next){*next=0;next+=3;}
        snprintf(fields[(*total)++],sizeof(fields[0]),"%s",part);part=next;
    }
    unsigned count=*total>TP_BAR_FOOTER_MAX?TP_BAR_FOOTER_MAX:*total;
    static uint32_t previous_fields,previous_height,previous_font;
    static int previous_width,previous_language=-1;
    if(previous_fields!=s_bars.fields||previous_height!=s_bars.footer_height||previous_font!=s_bars.footer_font_size||
       previous_width!=width||previous_language!=s_language){
        s_footer_font_size=s_bars.footer_font_size;
        lv_tiny_ttf_set_size(s_bar_font,s_footer_font_size);
        while(s_bar_font->line_height>s_bars.footer_height-8&&s_footer_font_size>8)
            lv_tiny_ttf_set_size(s_bar_font,--s_footer_font_size);
        previous_fields=s_bars.fields;previous_height=s_bars.footer_height;previous_font=s_bars.footer_font_size;
        previous_width=width;previous_language=s_language;
    }
    /* Keep every selected field; retain a reduced size until preferences or
     * orientation change instead of resizing with each telemetry value. */
    for(;;){
        s_footer_icon_size=s_footer_font_size*3/4;if(s_footer_icon_size<8)s_footer_icon_size=8;
        int measured=count?(count-1)*24:0;
        for(unsigned i=0;i<count;++i)measured+=footer_field_width(fields[i]);
        if(measured<=width||s_footer_font_size<=8)break;
        lv_tiny_ttf_set_size(s_bar_font,--s_footer_font_size);
    }
    text[0]=0;
    for(unsigned i=0;i<count;++i){size_t n=strlen(text);snprintf(text+n,capacity-n,"%s%.127s",i?" | ":"",fields[i]);}
    return count;
}

static void bar_single_line(char *text,size_t capacity,int width,const lv_font_t *font)
{
    (void)capacity;
    /* Loop counts and adjacent titles are atomic fields. Do not leave half
     * a label or hide its value behind an ellipsis at large font sizes. */
    while(text[0]&&bar_width(text,font)>width){
        char *last=NULL;
        for(char *part=strstr(text," | ");part;part=strstr(part+3," | "))last=part;
        if(last)*last=0;else text[0]=0;
    }
}

static void bars_update(const prompter_status_t *state)
{
    char left[96]="",date[32]="",clock[16]="",right[1536]="",heading[384]="",next[97],previous[97],name[33],mac[18];
    uint32_t fields=s_bars.fields;time_t utc=time(NULL),local=utc-device_settings_clock_offset()*60;struct tm tm;
    if(utc>=1577836800&&gmtime_r(&local,&tm)){
        strftime(date,sizeof(date),"%Y-%m-%d",&tm);strftime(clock,sizeof(clock),"%H:%M",&tm);
        if(fields&TP_BAR_DATE)strftime(left,sizeof(left),"%Y-%m-%d",&tm);
        if(fields&TP_BAR_TIME){size_t n=strlen(left);strftime(left+n,sizeof(left)-n,n?" %H:%M":"%H:%M",&tm);}
    }
    const bool guide=!state->length||s_guide_only;
    if(!guide&&(fields&TP_BAR_TIMECODE)){
        uint64_t seconds=state->playback_elapsed_ms/1000;
        snprintf(left,sizeof(left),"%02llu:%02llu:%02llu",(unsigned long long)(seconds/3600),
            (unsigned long long)(seconds/60%60),(unsigned long long)(seconds%60));
    }
    bar_text(s_header_left,left);
    const char *status=guide?(control_session_connected()?(s_language?"Connected":"已连接"):(s_language?"Waiting":"待连接")):
        state->countdown_remaining_ms>0?(s_language?"Ready":"准备中"):state->playing?(s_language?"Playing":"播放中"):state->stopped?(s_language?"Stopped":"已停止"):(s_language?"Paused":"已暂停");
    if(guide||(fields&TP_BAR_STATE))snprintf(heading,sizeof(heading),"%s",status);
    script_library_next_title(next,sizeof(next));
    script_library_previous_title(previous,sizeof(previous));
    device_settings_display_name(name,sizeof(name));
    wifi_settings_device_mac(mac,sizeof(mac));
    bar_text(s_title,(fields&TP_BAR_NAME)?name:"");
#define ADD_HEADING(fmt,...) do{size_t n=strlen(heading);snprintf(heading+n,sizeof(heading)-n,"%s" fmt,n?" | ":"",__VA_ARGS__);}while(0)
    if(!guide&&state->loop){
        if(fields&TP_BAR_LOOP)ADD_HEADING("%s%lu",s_language?"Loops: ":"已循环：",(unsigned long)state->loop_count);
    }else if(!guide){
        if((fields&TP_BAR_PREVIOUS)&&previous[0])ADD_HEADING("%s: %s",s_language?"Previous":"上一篇",previous);
        else if(state->auto_next&&(fields&TP_BAR_NEXT)&&next[0])ADD_HEADING("%s: %s",s_language?"Next":"下一篇",next);
    }
#undef ADD_HEADING
    bar_single_line(heading,sizeof(heading),lv_obj_get_width(s_header_right),s_info_font);
    bar_text(s_header_right,heading);
    lv_text_attributes_t attributes={0};const char *end=strchr(heading,'\n');
    const char *visible_heading=lv_label_get_text(s_header_right);
    int heading_width=lv_text_get_width(visible_heading,end?(uint32_t)(end-heading):(uint32_t)strlen(heading),s_info_font,&attributes);
    /* This object is aligned TOP_RIGHT: x is an offset from the right edge,
     * not an absolute screen coordinate. An absolute x pushed every state
     * icon (including the playing dot) completely outside the panel. */
    lv_obj_set_x(s_state_icon,-16-heading_width-8);
    bool icon_visible=!guide&&(fields&TP_BAR_STATE)&&s_bars.header&&!state->countdown_remaining_ms;
    lv_obj_set_hidden(s_timecode,true);
    bool playing=state->playing&&state->length&&!state->countdown_remaining_ms&&!s_guide_only;
    lv_obj_set_hidden(s_state_icon,!icon_visible);
    lv_obj_set_hidden(s_play_dot,!playing);
    bool stopped=state->stopped&&state->length&&!state->countdown_remaining_ms&&!playing&&!s_guide_only;
    lv_obj_set_hidden(s_stop_icon,!stopped);
    for(unsigned i=0;i<2;i++)lv_obj_set_hidden(s_pause_icon[i],playing||stopped);
    /* Keep a solid dot: whole-object opacity needs an unnecessary alpha layer. */
    bool pulse=icon_visible&&playing;
    if(pulse!=s_play_dot_active){s_play_dot_active=pulse;s_play_dot_started=lv_tick_get();s_play_dot_bright=false;}
    unsigned valid=0;for(unsigned i=0;i<s_tile_count;i++)if(s_tiles[i].valid)valid++;
    int extent=state->loop?s_loop_period:s_end_scroll;
    double progress=extent>0?fmin(100.0,100.0*s_scroll/extent):0;
#define ADD_FIELD(bit,fmt,...) do{if(fields&(bit)){size_t n=strlen(right);snprintf(right+n,sizeof(right)-n,"%s" fmt,n?" | ":"",__VA_ARGS__);}}while(0)
    ADD_FIELD(TP_BAR_SPEED,"x%d",state->shuttle?state->shuttle:1);
    ADD_FIELD(TP_BAR_FPS,"%.1f fps",s_fps);
    ADD_FIELD(TP_BAR_BUFFER,"%s %u/%u",s_language?"Buffer":"缓冲",valid,s_tile_count);
    ADD_FIELD(TP_BAR_JITTER,"%s %.1f ms",s_language?"Jitter":"抖动",s_jitter_ms);
    ADD_FIELD(TP_BAR_RENDER,"%s %.1f ms",s_language?"Render":"绘制",s_render_ms);
    ADD_FIELD(TP_BAR_PROGRESS,"%.0f%%",progress);
    ADD_FIELD(TP_BAR_MEMORY,"RAM %.1f/%.1f MB",heap_caps_get_free_size(MALLOC_CAP_SPIRAM)/1048576.0,heap_caps_get_total_size(MALLOC_CAP_SPIRAM)/1048576.0);
    ADD_FIELD(TP_BAR_FONT,"Aa %d px",state->font_size);
    ADD_FIELD(TP_BAR_BRIGHTNESS,"{sun} %d%%",state->brightness);
    ADD_FIELD(TP_BAR_LETTER,"%s %d px",s_language?"Letter":"字距",state->letter_spacing);
    ADD_FIELD(TP_BAR_LINE,"%s %d px",s_language?"Line":"行距",state->line_spacing);
    ADD_FIELD(TP_BAR_MARGIN,"%s %d/%d px",s_language?"Margins":"边距",state->margin_left,state->margin_right);
    ADD_FIELD(TP_BAR_COUNTDOWN,"{clk} %d s",state->countdown);
    ADD_FIELD(TP_BAR_BITRATE,"%s %.1f Mb/s",s_language?"Pixels":"像素码率",s_fps*s_width*s_height*16/1000000.0);
    ADD_FIELD(TP_BAR_DRAWN,"%s %llu",s_language?"Drawn":"绘制帧",(unsigned long long)s_drawn_frames);
    ADD_FIELD(TP_BAR_LATE,"%s %llu",s_language?"Late":"延迟帧",(unsigned long long)s_late_frames);
#undef ADD_FIELD
    unsigned footer_total=0,footer_shown=bar_fit(right,sizeof(right),lv_obj_get_width(s_footer_right),s_bar_font,&footer_total);
    int top,bottom;
    bar_text(s_footer_right,right);
    lv_obj_t *headers[]={s_title,s_header_left,s_header_right,s_timecode};
    for(unsigned i=0;i<4;++i){
        bar_ink(s_info_font,lv_label_get_text(headers[i]),&top,&bottom);
        lv_obj_set_y(headers[i],(s_bars.header_height-(bottom-top))/2-top);
    }
    bar_ink(s_info_font,lv_label_get_text(s_header_right),&top,&bottom);
    lv_obj_set_y(s_state_icon,lv_obj_get_y(s_header_right)+(top+bottom-lv_obj_get_height(s_state_icon))/2);
    cJSON *json=cJSON_CreateObject();if(!json)return;
    cJSON_AddBoolToObject(json,"ok",true);cJSON_AddStringToObject(json,"nextTitle",next);
    cJSON_AddStringToObject(json,"previousTitle",previous);cJSON_AddStringToObject(json,"deviceName",name);cJSON_AddStringToObject(json,"mac",mac);
    cJSON_AddStringToObject(json,"dateTime",left);cJSON_AddStringToObject(json,"headerState",lv_label_get_text(s_header_right));
    cJSON_AddStringToObject(json,"clockDate",date);cJSON_AddStringToObject(json,"clockTime",clock);
    cJSON_AddStringToObject(json,"footerText",right);
    cJSON_AddNumberToObject(json,"footerShown",footer_shown);cJSON_AddNumberToObject(json,"footerSelected",footer_total);
    cJSON_AddNumberToObject(json,"footerFontSize",s_footer_font_size);
    cJSON_AddNumberToObject(json,"fps",s_fps);cJSON_AddNumberToObject(json,"jitterMs",s_jitter_ms);
    cJSON_AddNumberToObject(json,"renderMs",s_render_ms);cJSON_AddNumberToObject(json,"bufferReady",valid);
    cJSON_AddNumberToObject(json,"bufferTotal",s_tile_count);cJSON_AddNumberToObject(json,"scroll",s_scroll);
    cJSON_AddNumberToObject(json,"freeMemory",heap_caps_get_free_size(MALLOC_CAP_SPIRAM));cJSON_AddNumberToObject(json,"totalMemory",heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    cJSON_AddNumberToObject(json,"drawnFrames",(double)s_drawn_frames);cJSON_AddNumberToObject(json,"lateFrames",(double)s_late_frames);
    cJSON_AddNumberToObject(json,"pixelBitrate",s_fps*s_width*s_height*16);
    xSemaphoreTake(s_preview_mutex,portMAX_DELAY);cJSON_PrintPreallocated(json,s_telemetry,sizeof(s_telemetry),false);xSemaphoreGive(s_preview_mutex);cJSON_Delete(json);
}

/* Export the exact LVGL line breaks and rounded glyph advances once per layout.
 * HTTP/BLE tasks only copy this immutable snapshot; they never call LVGL. */
static void preview_layout_build(const prompter_status_t *state)
{
    cJSON *root=cJSON_CreateObject();
    cJSON_AddBoolToObject(root,"ok",true);
    cJSON_AddNumberToObject(root,"revision",++s_preview_revision);
    cJSON_AddNumberToObject(root,"scriptRevision",state->script_revision);
    cJSON_AddNumberToObject(root,"width",s_width);
    cJSON_AddNumberToObject(root,"height",s_height);
    cJSON_AddNumberToObject(root,"viewportTop",VIEWPORT_TOP);
    cJSON_AddNumberToObject(root,"textTop",s_text_origin);
    cJSON_AddNumberToObject(root,"lineHeight",s_font->line_height);
    cJSON_AddNumberToObject(root,"baseline",s_font->line_height-s_font->base_line);
    cJSON_AddNumberToObject(root,"font",state->font_size);
    cJSON_AddNumberToObject(root,"lineSpacing",state->line_spacing);
    cJSON_AddNumberToObject(root,"letterSpacing",state->letter_spacing);
    cJSON_AddNumberToObject(root,"rotation",state->rotation);
    cJSON_AddNumberToObject(root,"mirror",state->mirror);
    cJSON_AddNumberToObject(root,"textAlign",state->text_align);
    cJSON_AddNumberToObject(root,"marginLeft",state->margin_left);
    cJSON_AddNumberToObject(root,"marginRight",state->margin_right);
    cJSON_AddNumberToObject(root,"bandTop",s_band_top);
    cJSON_AddNumberToObject(root,"bandHeight",s_band_bottom-s_band_top);
    cJSON_AddStringToObject(root,"text",s_text);
    cJSON *lines=cJSON_AddArrayToObject(root,"lines");
    lv_text_attributes_t attributes={.letter_space=s_letter_spacing,.max_width=s_width,.text_flags=LV_TEXT_FLAG_NONE};
    uint32_t offset=0,total=strlen(s_text);
    s_row_count=0;
    while(offset<total){
        uint32_t count=lv_text_get_next_line(s_text+offset,total-offset,s_font,NULL,&attributes);
        if(!count)break;
        int values[3]={(int)offset,(int)count,lv_text_get_line_width(s_text+offset,count,s_font,&attributes)};
        s_row_offsets[s_row_count++]=offset;
        cJSON_AddItemToArray(lines,cJSON_CreateIntArray(values,3));offset+=count;
    }
    s_row_offsets[s_row_count]=offset;
    /* Ignore trailing blank lines for playback, without changing the SD text. */
    s_playback_rows=s_row_count;
    while(s_playback_rows){
        bool ink=false;
        for(uint32_t i=s_row_offsets[s_playback_rows-1];i<s_row_offsets[s_playback_rows];++i)
            if((unsigned char)s_text[i]>' '){ink=true;break;}
        if(ink)break;
        --s_playback_rows;
    }
    int step=s_font->line_height+s_line_spacing;
    s_loop_period=(s_playback_rows?s_playback_rows:1)*step;
    int reading_center=(state->mirror&2)?s_height-(s_band_top+s_band_bottom)/2:(s_band_top+s_band_bottom)/2;
    int lead_in=s_text_origin-(reading_center-s_font->line_height/2);
    s_end_scroll=s_playback_rows?(s_playback_rows-1)*step+lead_in:0;
    cJSON_AddNumberToObject(root,"playbackRows",s_playback_rows);
    cJSON_AddNumberToObject(root,"loopPeriod",s_loop_period);
    cJSON_AddNumberToObject(root,"endScroll",s_end_scroll);
    cJSON *advances=cJSON_AddArrayToObject(root,"advances");
    uint32_t *seen=calloc(total+1,sizeof(uint32_t));unsigned used=0;
    if(seen)for(offset=0;offset<total;){
        uint32_t cp,next;lv_text_encoded_letter_next_2(s_text,&cp,&next,&offset);
        bool found=false;for(unsigned i=0;i<used;++i)if(seen[i]==cp){found=true;break;}
        if(found)continue;
        seen[used++]=cp;
        lv_font_glyph_dsc_t glyph={0};s_font_source->get_glyph_dsc(s_font_source,&glyph,cp,0);
        int values[3]={(int)cp,lv_font_get_glyph_width(s_font,cp,0),glyph.box_w?glyph.ofs_x:0};
        cJSON_AddItemToArray(advances,cJSON_CreateIntArray(values,3));
        /* Rasterize unique glyphs during layout, before playback starts. */
        if(glyph.box_w&&glyph.box_h){
            glyph.resolved_font=s_font_source;
            s_font_source->get_glyph_bitmap(&glyph,NULL);
            s_font_source->release_glyph(s_font_source,&glyph);
        }
    }
    free(seen);
    char *json=cJSON_PrintUnformatted(root);cJSON_Delete(root);
    xSemaphoreTake(s_preview_mutex,portMAX_DELAY);
    free(s_preview_json);s_preview_json=json;
    xSemaphoreGive(s_preview_mutex);
}

static int floor_div(int value, int divisor)
{
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

static void mirror_pixels(lv_draw_buf_t *buffer,int width,int height,int mirror)
{
    if(mirror&1)for(int y=0;y<height;++y){
        uint16_t *row=(uint16_t *)(buffer->data+y*buffer->header.stride);
        for(int x=0;x<width/2;++x){uint16_t pixel=row[x];row[x]=row[width-1-x];row[width-1-x]=pixel;}
    }
    if(mirror&2)for(int y=0;y<height/2;++y){
        uint16_t *a=(uint16_t *)(buffer->data+y*buffer->header.stride);
        uint16_t *b=(uint16_t *)(buffer->data+(height-1-y)*buffer->header.stride);
        for(int x=0;x<width;++x){uint16_t pixel=a[x];a[x]=b[x];b[x]=pixel;}
    }
}

/* Small independent LVGL canvas: the reader sees the same reflected direction
 * as the script. Never draw with the CPU into a live panel framebuffer. */
static void countdown_build(const prompter_status_t *state,uint32_t now)
{
    memset(s_countdown_buffer->data,0,s_countdown_buffer->data_size);
    lv_layer_t layer;lv_canvas_init_layer(s_countdown_canvas,&layer);
    int digit=(state->countdown_remaining_ms+999)/1000;
    if(digit!=s_countdown_digit){s_countdown_digit=digit;s_digit_started=now;}
    int elapsed=now-s_digit_started;
    if(elapsed>220)elapsed=220;
    int ease=220-(220-elapsed)*(220-elapsed)/220;
    lv_draw_rect_dsc_t disc;lv_draw_rect_dsc_init(&disc);
    disc.radius=LV_RADIUS_CIRCLE;disc.bg_color=lv_color_hex(state->text_color);
    disc.bg_opa=LV_OPA_COVER;
    lv_area_t bounds={15,15,304,304};lv_draw_rect(&layer,&disc,&bounds);
    lv_draw_arc_dsc_t arc;lv_draw_arc_dsc_init(&arc);
    arc.center=(lv_point_t){160,160};arc.radius=151;arc.width=3;
    arc.color=lv_color_hex(state->text_color);arc.opa=75;
    arc.start_angle=0;arc.end_angle=360;lv_draw_arc(&layer,&arc);
    arc.radius=155;arc.width=4;arc.opa=LV_OPA_COVER;arc.rounded=1;
    arc.start_angle=270;
    int angle=state->countdown>0?(int)((int64_t)state->countdown_remaining_ms*360/(state->countdown*1000)):0;
    if(angle>360)angle=360;
    if(angle<1)angle=1;
    arc.end_angle=270+angle;lv_draw_arc(&layer,&arc);
    int font_size=state->font_size*3/2;
    if(font_size<96)font_size=96;
    if(font_size>150)font_size=150;
    font_size=font_size*(88+12*ease/220)/100;
    if(font_size!=s_countdown_font_size){lv_tiny_ttf_set_size(s_countdown_font,font_size);s_countdown_font_size=font_size;}
    char number[16];snprintf(number,sizeof(number),"%d",(state->countdown_remaining_ms+999)/1000);
    lv_point_t size;lv_text_get_size(&size,number,s_countdown_font,0,0,COUNTDOWN_SIZE,LV_TEXT_FLAG_NONE);
    lv_draw_label_dsc_t label;lv_draw_label_dsc_init(&label);
    uint32_t color=state->text_color;
    int luma=((color>>16)&255)*299+((color>>8)&255)*587+(color&255)*114;
    label.text=number;label.font=s_countdown_font;label.color=luma>128000?lv_color_hex(0x10171c):lv_color_white();
    label.opa=80+175*ease/220;
    label.align=LV_TEXT_ALIGN_CENTER;
    lv_area_t area={0,160-size.y/2,319,160+(size.y+1)/2};lv_draw_label(&layer,&label,&area);
    lv_canvas_finish_layer(s_countdown_canvas,&layer);
    mirror_pixels(s_countdown_buffer,COUNTDOWN_SIZE,COUNTDOWN_SIZE,state->mirror);
    if(!state->rotation)for(int row=0;row<COUNTDOWN_SIZE;++row)
        memcpy(s_countdown_pixels+row*COUNTDOWN_SIZE,s_countdown_buffer->data+row*s_countdown_buffer->header.stride,COUNTDOWN_SIZE*2);
    else lv_draw_rotate(s_countdown_buffer->data,s_countdown_pixels,COUNTDOWN_SIZE,COUNTDOWN_SIZE,
                   s_countdown_buffer->header.stride,COUNTDOWN_SIZE*2,
                   (lv_display_rotation_t)(state->rotation/90),LV_COLOR_FORMAT_RGB565);
    ESP_ERROR_CHECK(esp_cache_msync(s_countdown_pixels,COUNTDOWN_SIZE*COUNTDOWN_SIZE*2,ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    s_countdown_frame=now;s_force_present=true;
}

static const char *display_text(void)
{
    /* Operator instructions have their own fixed-size LVGL card. Only the
     * user's script belongs in the styled/mirrored tile renderer. */
    return s_text;
}

static void welcome_content_layout(bool landscape)
{
    const tp_guide_copy_t *copy=&tp_guide_copy[s_guide_tracker.stage];
    bool qr=!lv_obj_is_hidden(s_welcome_visual);
    lv_obj_set_width(s_welcome_content,lv_pct(100));
    lv_obj_update_layout(s_welcome_card);
    int width=lv_obj_get_content_width(s_welcome_content);
    if(width>TP_GUIDE_CONTENT_WIDTH)width=TP_GUIDE_CONTENT_WIDTH;
    int max_copy_width=width-(landscape&&qr?TP_GUIDE_QR_SIZE+TP_GUIDE_COLUMN_GAP:0);
    lv_point_t body_size,title_size;
    lv_text_get_size(&body_size,s_welcome_body_text,s_guide_font,0,
                     TP_GUIDE_BODY_LINE_HEIGHT-s_guide_font->line_height,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    lv_text_get_size(&title_size,lv_label_get_text(s_welcome_title),s_ready_font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    int copy_width=body_size.x>title_size.x?body_size.x:title_size.x;
    if(copy->loading||s_guide_tracker.stage==TP_GUIDE_READY||s_guide_tracker.stage==TP_GUIDE_RECONNECT){
        int title_width=title_size.x+32+14;
        if(title_width>copy_width)copy_width=title_width;
    }
    if(copy_width>max_copy_width)copy_width=max_copy_width;
    if(copy_width<1)copy_width=1;
    lv_obj_set_width(s_welcome_content,landscape&&qr?copy_width+TP_GUIDE_QR_SIZE+TP_GUIDE_COLUMN_GAP:width);
    lv_obj_set_flex_grow(s_welcome_copy,0);
    lv_obj_set_width(s_welcome_copy,copy_width);
    lv_obj_set_height(s_welcome_copy,LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(s_welcome_copy,copy->centered||!landscape?LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT,0);
    lv_obj_set_flex_align(s_welcome_title_row,copy->centered||!landscape?LV_FLEX_ALIGN_CENTER:LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_update_layout(s_welcome_card);
}

static void welcome_update(const char *address)
{
    (void)address;
    tp_guide_input_t input;wifi_settings_guide_snapshot(&input);
    tp_guide_stage_t before=s_guide_tracker.stage;
    bool initial=!s_guide_tracker.initialized;
    tp_guide_stage_t stage=tp_guide_stage(&s_guide_tracker,&input,lv_tick_get());
    const tp_guide_stage_t content_stage=tp_guide_content_stage(stage,&input);
    const tp_guide_copy_t *copy=&tp_guide_copy[content_stage];
    bool standby=stage==TP_GUIDE_READY||stage==TP_GUIDE_RECONNECT;
    const char *title=tp_guide_copy[stage].title[s_language];
    char name[33],ble[30],payload[256],body[1024],display_url[256];
    device_settings_boot_names(name,sizeof(name),ble,sizeof(ble));
    bool bluetooth=content_stage==TP_GUIDE_BLUETOOTH;
    const char *url=bluetooth?"https://0047ol.github.io/openprompt-p4/":NULL;
    if(url)snprintf(payload,sizeof(payload),"%s",url);
    else snprintf(payload,sizeof(payload),"http://%s/",input.local_domain[0]?input.local_domain:content_stage==TP_GUIDE_LAN&&input.wifi_connected&&input.address[0]?input.address:input.hotspot_address[0]?input.hotspot_address:"192.168.4.1");
    if(url)snprintf(display_url,sizeof(display_url),"%s",url);
    else snprintf(display_url,sizeof(display_url),"http://%s/",content_stage==TP_GUIDE_LAN&&input.wifi_connected&&input.address[0]?input.address:input.hotspot_address[0]?input.hotspot_address:"192.168.4.1");
    tp_guide_format_body(body,sizeof(body),copy->body[s_language],name,
                         input.wifi_connected?input.ssid:name,ble,display_url);
    if(stage==TP_GUIDE_JOIN){
        char password[64];device_settings_boot_password(password,sizeof(password));
        bool encoded=tp_guide_wifi_payload(payload,sizeof(payload),name,password);
        memset(password,0,sizeof(password));
        if(!encoded)payload[0]=0;
    }
    bool qr=copy->qr&&payload[0];
    bool stage_changed=before!=stage&&!(standby&&(before==TP_GUIDE_READY||before==TP_GUIDE_RECONNECT));
    bool changed=initial||stage_changed||strcmp(s_welcome,payload)||
        strcmp(lv_label_get_text(s_welcome_title),title)||
        strcmp(s_welcome_body_text,body)||
        lv_obj_is_hidden(s_welcome_visual)==qr;
    if(!changed)return;
    if(strcmp(s_welcome_qr_payload,payload)&&qr){
        if(lv_qrcode_update(s_welcome_qr,payload,strlen(payload))!=LV_RESULT_OK){qr=false;ESP_LOGW(TAG,"Connection QR encode failed");}
        else snprintf(s_welcome_qr_payload,sizeof(s_welcome_qr_payload),"%s",payload);
    }
    snprintf(s_welcome,sizeof(s_welcome),"%s",payload);
    lv_obj_set_hidden(s_welcome_visual,!qr);
    lv_label_set_text(s_welcome_title,title);
    snprintf(s_welcome_body_text,sizeof(s_welcome_body_text),"%s",body);
    char *second=strchr(body,'\n');
    if(second)*second++='\0';
    lv_label_set_text(s_welcome_body,body);
    lv_label_set_text(s_welcome_body_more,second?second:"");
    lv_obj_set_hidden(s_welcome_body_more,!second||!*second);
    lv_obj_set_hidden(s_welcome_description,!body[0]&&(!second||!*second));
    lv_obj_set_hidden(s_welcome_loader,!copy->loading);
    welcome_content_layout(s_rotation==90||s_rotation==270);
    if(copy->loading&&(!s_text[0]||s_guide_only))ui_motion_spin(s_welcome_loader);else ui_motion_stop(s_welcome_loader);
    for(unsigned i=0;i<TP_GUIDE_STEP_COUNT;i++){
        bool active=i+1<=(standby?TP_GUIDE_STEP_COUNT:copy->step);
        lv_obj_set_style_bg_color(s_step_dot[i],lv_color_hex(active?0x243f50:0x1b262d),0);
        lv_obj_set_style_text_color(s_step_number[i],lv_color_hex(active?0xa1d9f6:0x798c98),0);
        lv_obj_set_style_text_color(s_step_label[i],lv_color_hex(active?0xeaf0f1:0x798c98),0);
        lv_label_set_text(s_step_label[i],tp_guide_steps[i][s_language]);
    }
    if(!s_text[0]||s_guide_only){
        s_guide_animation_pending=true;
    }
    lv_obj_update_layout(s_welcome_card);lv_obj_center(s_welcome_card);s_force_present=true;
}

static script_tile_t *tile_find(int index)
{
    /* All space outside the document shares an immutable black tile. */
    if(index<0 || index*TILE_HEIGHT>=s_text_height)return &s_empty_tile;
    for (unsigned i = 0; i < s_tile_count; ++i)
        if (s_tiles[i].valid && s_tiles[i].index == index) return &s_tiles[i];
    return NULL;
}

/* Highlight changes only which cached pixels are copied. Keep companion
 * buffers across toggles and derive them from already rendered RGB565 tiles;
 * rebuilding every glyph/canvas here caused allocation and render spikes.
 * Allocation failure falls back to readable, undimmed text, never a reboot. */
static bool tile_ensure_dim(script_tile_t *tile)
{
    if(tile->dim_pixels)return true;
    tile->dim_pixels=heap_caps_aligned_alloc(64,1280*TILE_HEIGHT*2,MALLOC_CAP_SPIRAM);
    return tile->dim_pixels!=NULL;
}

static void highlight_update(bool enabled)
{
    s_center_highlight=enabled;
    /* A half-built tile has no complete dim image. Restart its prefetch so
     * switching midway cannot publish uninitialized rows. */
    s_preparing_index=INT32_MIN;s_preparing_y=0;s_preparing_tile=NULL;
    if(enabled)for(unsigned i=0;i<s_tile_count;++i){
        script_tile_t *tile=&s_tiles[i];
        if(!tile_ensure_dim(tile)){
            ESP_LOGW(TAG,"Highlight buffer unavailable for tile %u; retaining readable text",i);
            continue;
        }
        if(!tile->valid)continue;
        for(int p=0;p<s_width*TILE_HEIGHT;++p)tile->dim_pixels[p]=(tile->pixels[p]>>1)&0x7bef;
        ESP_ERROR_CHECK(esp_cache_msync(tile->dim_pixels,s_width*TILE_HEIGHT*2,ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    }
    s_force_present=true;
}

static void tile_transform(script_tile_t *tile,int first_y,int end_y)
{
    const int block=16,w=s_width,h=TILE_HEIGHT;
    if(s_rotation==90||s_rotation==270){
        /* Transpose a small block through internal RAM. Write a complete
         * 64-byte destination cache line before moving on; eight-row writes
         * evicted each partially written PSRAM line four times per tile. */
        int stride=s_canvas_buffer->header.stride/2;
        const uint16_t *source=(const uint16_t *)s_canvas_buffer->data;
        uint16_t block_pixels[32][16];
        for(int by=first_y;by<end_y;by+=32)for(int bx=0;bx<w;bx+=16){
          int rows=end_y-by<32?end_y-by:32,cols=w-bx<16?w-bx:16;
          for(int y=0;y<rows;++y)memcpy(block_pixels[y],source+(by+y)*stride+bx,cols*2);
          for(int x=bx;x<bx+cols;++x){
            int mx=(s_mirror&1)?w-1-x:x;
            int my=(s_mirror&2)?h-1-by:by;
            int target=s_rotation==90?(w-1-mx)*h+my:mx*h+h-1-my;
            int increment=(s_rotation==90?1:-1)*((s_mirror&2)?-1:1);
            for(int y=0;y<rows;++y,target+=increment){
                uint16_t pixel=block_pixels[y][x-bx];tile->pixels[target]=pixel;
                if(s_center_highlight&&tile->dim_pixels)tile->dim_pixels[target]=(pixel>>1)&0x7bef;
            }
          }
        }
        return;
    }
    for(int by=first_y;by<end_y;by+=block)for(int bx=0;bx<w;bx+=block){
        int ey=by+block<end_y?by+block:end_y,ex=bx+block<w?bx+block:w;
        for(int y=by;y<ey;++y){
            const uint16_t *row=(const uint16_t *)(s_canvas_buffer->data+y*s_canvas_buffer->header.stride);
            for(int x=bx;x<ex;++x){
                int mx=(s_mirror&1)?w-1-x:x,my=(s_mirror&2)?h-1-y:y;
                int target=s_rotation==90?(w-1-mx)*h+my:
                           s_rotation==270?mx*h+h-1-my:
                           s_rotation==180?(h-1-my)*w+w-1-mx:my*w+mx;
                uint16_t pixel=row[x];tile->pixels[target]=pixel;
                if(s_center_highlight&&tile->dim_pixels)tile->dim_pixels[target]=(pixel>>1)&0x7bef;
            }
        }
    }
}

static script_tile_t *tile_build_piece(int index,int first_y,int rows)
{
    int64_t started = esp_timer_get_time();
    if(index<0 || index*TILE_HEIGHT>=s_text_height)return &s_empty_tile;
    script_tile_t *tile=first_y?s_preparing_tile:NULL;
    if(!tile){
        /* Pin every visible source tile, including both sides of a loop join.
         * A modulo ring can otherwise overwrite the tail with the head. */
        for(unsigned i=0;i<s_tile_count;++i)if(s_tiles[i].valid&&s_tiles[i].index==index){tile=&s_tiles[i];break;}
        for(unsigned n=0;!tile&&n<s_tile_count;++n){
            unsigned i=(s_evict_slot+n)%s_tile_count;bool pinned=false;
            for(unsigned p=0;s_tiles[i].valid&&p<s_frame_tile_count;++p)
                if(s_tiles[i].index==s_frame_tiles[p]){pinned=true;break;}
            if(!pinned){tile=&s_tiles[i];s_evict_slot=(i+1)%s_tile_count;}
        }
        ESP_ERROR_CHECK(tile?ESP_OK:ESP_ERR_NO_MEM);
        if(rows<TILE_HEIGHT)s_preparing_tile=tile;
    }
    int start = index * TILE_HEIGHT;
    int end_y=first_y+rows;
    if(!first_y)tile->valid=false;
    memset(s_canvas_buffer->data+first_y*s_canvas_buffer->header.stride,0,rows*s_canvas_buffer->header.stride);
    if (start + TILE_HEIGHT > 0 && start < s_text_height) {
        lv_layer_t layer;
        lv_draw_label_dsc_t dsc;
        lv_draw_label_dsc_init(&dsc);
        dsc.font = s_font; dsc.text_static = 1;
        dsc.letter_space = s_letter_spacing; dsc.line_space = s_line_spacing;
        dsc.color = lv_color_hex(s_text_color);
        dsc.align = s_text_align==1?LV_TEXT_ALIGN_CENTER:s_text_align==2?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT;
        int step=s_font->line_height+s_line_spacing;
        unsigned first=start+first_y>0?(unsigned)((start+first_y)/step):0;
        unsigned last=(unsigned)((start+end_y-1)/step);
        if(!s_repeat_tile_span&&last>=s_row_count)last=s_row_count?s_row_count-1:0;
        for(unsigned row=first;s_row_count&&row<=last;++row){
            unsigned text_row=s_repeat_tile_span?row%s_playback_rows:row;
            uint32_t begin=s_row_offsets[text_row],end=s_row_offsets[text_row+1];
            while(end>begin&&(s_text[end-1]=='\n'||s_text[end-1]=='\r'))--end;
            if(end==begin)continue;
            /* Finish the static-pointer draw before restoring the terminator. */
            char saved=s_text[end];s_text[end]=0;
            lv_canvas_init_layer(s_canvas,&layer);
            layer._clip_area.y1=first_y;layer._clip_area.y2=end_y-1;
            dsc.text=s_text+begin;dsc.text_length=end-begin;
            lv_area_t area={0,(int)row*step-start,s_width-1,(int)row*step-start+s_font->line_height-1};
            lv_draw_label(&layer,&dsc,&area);
            lv_canvas_finish_layer(s_canvas,&layer);
            s_text[end]=saved;
        }
    }
    if(s_bold){
        /* Thicken the cached glyph mask once, not on every scrolling frame.
         * Expanding right by one pixel preserves all advances and line breaks. */
        for(int y=first_y;y<end_y;++y){
            uint16_t *row=(uint16_t *)(s_canvas_buffer->data+y*s_canvas_buffer->header.stride);
            for(int x=s_width-1;x>0;--x){
                uint16_t left=row[x-1],right=row[x];
                row[x]=(((left&0xf800)>(right&0xf800)?left:right)&0xf800)|
                       (((left&0x07e0)>(right&0x07e0)?left:right)&0x07e0)|
                       (((left&0x001f)>(right&0x001f)?left:right)&0x001f);
            }
        }
    }
    int64_t painted=esp_timer_get_time();
    tile_transform(tile,first_y,end_y);
    int64_t transformed=esp_timer_get_time();
    if(end_y==TILE_HEIGHT)ESP_ERROR_CHECK(esp_cache_msync(tile->pixels, s_width * TILE_HEIGHT * sizeof(uint16_t),
                                    ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    if(s_center_highlight&&tile->dim_pixels&&end_y==TILE_HEIGHT) {
        ESP_ERROR_CHECK(esp_cache_msync(tile->dim_pixels,s_width*TILE_HEIGHT*2,ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    }
    if(esp_timer_get_time()-started>30000)
        ESP_LOGI(TAG,"TILE_WORK index=%d paint_us=%ld transform_us=%ld sync_us=%ld",index,
                 (long)(painted-started),(long)(transformed-painted),(long)(esp_timer_get_time()-transformed));
    tile->index = index; tile->valid = end_y==TILE_HEIGHT;
    s_build_us += esp_timer_get_time() - started;
    if(tile->valid)++s_built_tiles;
    return tile;
}
static script_tile_t *tile_build(int index){return tile_build_piece(index,0,TILE_HEIGHT);}

static void cache_reset(const prompter_status_t *state, bool rebuild_layout, float position)
{
    s_rotation = state->rotation; s_letter_spacing = state->letter_spacing; s_line_spacing = state->line_spacing;
    s_text_color=state->text_color; s_text_align=state->text_align; s_mirror=state->mirror;
    s_center_highlight=state->center_highlight;
    s_bold=state->bold;
    s_preparing_index=INT32_MIN;s_preparing_y=0;
    s_preparing_tile=NULL;s_frame_tile_count=0;
    /* Retain short documents entirely when memory permits. Long documents
     * still use a bounded ring, and four MiB remain available to the system. */
    lv_point_t size;
    lv_text_get_size(&size, display_text(), s_font, s_letter_spacing, s_line_spacing,
                     s_width, LV_TEXT_FLAG_NONE);
    s_text_height = size.y;
    unsigned needed=(s_text_height+TILE_HEIGHT-1)/TILE_HEIGHT;
    unsigned wanted=needed>TILE_COUNT?TILE_BASE_COUNT:needed<TILE_BASE_COUNT?TILE_BASE_COUNT:needed;
    size_t tile_bytes=1280*TILE_HEIGHT*2;
    for(unsigned i=0;i<TILE_COUNT;++i){
        if(i>=wanted){heap_caps_free(s_tiles[i].pixels);s_tiles[i].pixels=NULL;}
        if(i>=wanted){heap_caps_free(s_tiles[i].dim_pixels);s_tiles[i].dim_pixels=NULL;}
        s_tiles[i].valid=false;
    }
    s_tile_count=0;
    for (unsigned i = 0; i < wanted; ++i) {
        size_t extra=(!s_tiles[i].pixels?tile_bytes:0)+(s_center_highlight&&!s_tiles[i].dim_pixels?tile_bytes:0);
        if(i>=TILE_BASE_COUNT && heap_caps_get_free_size(MALLOC_CAP_SPIRAM)<extra+TILE_MEMORY_RESERVE)break;
        if (!s_tiles[i].pixels)
            s_tiles[i].pixels = heap_caps_aligned_alloc(64, tile_bytes, MALLOC_CAP_SPIRAM);
        if(!s_tiles[i].pixels)break;
        if(s_center_highlight)tile_ensure_dim(&s_tiles[i]);
        ++s_tile_count;
    }
    for(unsigned i=s_tile_count;i<TILE_COUNT;++i){
        heap_caps_free(s_tiles[i].pixels);s_tiles[i].pixels=NULL;
        heap_caps_free(s_tiles[i].dim_pixels);s_tiles[i].dim_pixels=NULL;
    }
    ESP_ERROR_CHECK(s_tile_count>=8?ESP_OK:ESP_ERR_NO_MEM);
    s_cache_all=needed<=s_tile_count;
    s_empty_tile=(script_tile_t){.valid=true,.pixels=s_blank_tile,.dim_pixels=s_blank_tile};
    if(!s_canvas_buffer||s_canvas_buffer->header.w!=s_width){
        lv_draw_buf_t *previous = s_canvas_buffer;
        s_canvas_buffer = lv_draw_buf_create(s_width, TILE_HEIGHT, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
        ESP_ERROR_CHECK(s_canvas_buffer ? ESP_OK : ESP_ERR_NO_MEM);
        lv_canvas_set_draw_buf(s_canvas, s_canvas_buffer);
        if (previous) lv_draw_buf_destroy(previous);
    }
    if(rebuild_layout)preview_layout_build(state);
    /* Expand tiny loops in the existing tile. Copying a 23 px period fifty
     * times per frame costs more DMA setup than the actual pixels. */
    s_repeat_tile_span=state->loop&&s_playback_rows&&s_loop_period<TILE_HEIGHT?
        (TILE_HEIGHT/s_loop_period)*s_loop_period:0;
    s_scroll = position; s_last_y = INT32_MIN; s_force_present = true;
    /* Prepare the complete visible frame and two look-ahead tiles. Building
     * all twelve before presenting delays every font/spacing/color edit. */
    int y=s_text_origin-(int)s_scroll;
    int first=floor_div(-y,TILE_HEIGHT);
    int last=floor_div(s_height-1-y,TILE_HEIGHT);
    for(int i=first;i<=last+2;++i)tile_build(i);
    s_tick = lv_tick_get();
    ESP_LOGI(TAG, "SCROLL_READY width=%d height=%d text_height=%d rotation=%d font=%d",
             s_width, s_height, s_text_height, s_rotation, state->font_size);
    ESP_LOGI(TAG,"SCROLL_CACHE tiles=%u document_tiles=%u complete_cache=%d free_psram=%u free_internal=%u",
             s_tile_count,needed,s_cache_all,(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
}

static void blit_text_segment(const uint16_t *pixels,int destination,int count,int offset)
{
    while(count>0){
        tp_guide_clip_t clip=tp_guide_clip(destination,count,s_band_top,s_band_bottom,
                                          s_sight_guide&&!s_countdown_shown);
        int rows=clip.rows,inset=clip.inset;
        if(!clip.skip){
            int left=s_margin_left+inset,width=s_width-2*inset;
            int logical_y=VIEWPORT_TOP+destination;
            if(s_rotation==90)
                display_port_blit(logical_y,1280-left-width,rows,width,pixels,TILE_HEIGHT,s_width,offset,inset);
            else if(s_rotation==270)
                display_port_blit(800-logical_y-rows,left,rows,width,pixels,TILE_HEIGHT,s_width,TILE_HEIGHT-offset-rows,inset);
            else if(s_rotation==180)
                display_port_blit(800-left-width,1280-logical_y-rows,width,rows,pixels,s_width,TILE_HEIGHT,inset,TILE_HEIGHT-offset-rows);
            else display_port_blit(left,logical_y,width,rows,pixels,s_width,TILE_HEIGHT,inset,offset);
        }
        destination+=rows;offset+=rows;count-=rows;
    }
}

static void guide_strip(int x,int y,int width,int height)
{
    int px=x,py=y,w=width,h=height;
    if(s_rotation==90){px=y;py=1280-x-width;w=height;h=width;}
    else if(s_rotation==270){px=800-y-height;py=x;w=height;h=width;}
    else if(s_rotation==180){px=800-x-width;py=1280-y-height;}
    if(s_guide_repaint||display_port_frame_region_changed(px,py,w,h))
        display_port_blit(px,py,w,h,s_guide_pixels,w,h,0,0);
}

typedef struct {int index,offset,destination,count;} visible_piece_t;
static visible_piece_t visible_piece(int y,int row)
{
    int source=row-y,count=s_height-row,index=-1,offset=0;
    bool repeat=s_previous.loop&&s_playback_rows;
    if(repeat&&(source>=0||s_previous.loop_count)){
        if(s_repeat_tile_span){
            source=((source%s_repeat_tile_span)+s_repeat_tile_span)%s_repeat_tile_span;
            if(count>s_repeat_tile_span-source)count=s_repeat_tile_span-source;
            return (visible_piece_t){0,source,row,count};
        }
        source=((source%s_loop_period)+s_loop_period)%s_loop_period;
        if(count>s_loop_period-source)count=s_loop_period-source;
    }
    if(source<0){if(count>-source)count=-source;}
    else if(source<s_text_height&&(!repeat||source<s_loop_period)){
        index=source/TILE_HEIGHT;offset=source%TILE_HEIGHT;
        if(count>s_text_height-source)count=s_text_height-source;
    }
    if(count>TILE_HEIGHT-offset)count=TILE_HEIGHT-offset;
    return (visible_piece_t){index,offset,row,count};
}

static void prepare_tiles(int first_index,int last_index)
{
    /* The current frame pins every DMA source, including both loop ends.
     * Look-ahead may only build a different tile. */
    int edge=s_previous.shuttle<0?first_index:last_index;
    int ahead=edge+(s_previous.shuttle<0?-2:2);
    if(s_previous.loop&&s_loop_period>0){int total=(s_loop_period+TILE_HEIGHT-1)/TILE_HEIGHT;ahead=(ahead%total+total)%total;}
    /* Populate every missing tile in a short document even while paused.
     * Once warm, playback and loop restarts need no glyph work at all. */
    if(s_cache_all){
        if(tile_find(ahead))for(int i=0;i*TILE_HEIGHT<s_text_height;++i)
            if(!tile_find(i)){ahead=i;break;}
    }
    if(s_preparing_index!=INT32_MIN&&tile_find(s_preparing_index))s_preparing_index=INT32_MIN;
    if(s_preparing_index==INT32_MIN&&!tile_find(ahead)){
        s_preparing_index=ahead;s_preparing_y=0;
    }
    if(s_preparing_index!=INT32_MIN){
        for(unsigned i=0;i<s_frame_tile_count;++i)
            if(s_frame_tiles[i]==s_preparing_index)return;
        /* Spread look-ahead work across frames. A new tile must never block
         * scrolling for its entire rasterization/rotation time. */
        int64_t prepare_started=esp_timer_get_time();
        /* Complete 64-byte rotated cache lines. Eight-row writes revisit
         * each PSRAM line four times; overlap this bounded work with DMA
         * instead of fragmenting it into inefficient partial writes. */
        int velocity=abs(s_previous.speed*(s_previous.shuttle?s_previous.shuttle:1));
        int rows=velocity>=512?64:TILE_PREPARE_ROWS;
        if(rows>TILE_HEIGHT-s_preparing_y)rows=TILE_HEIGHT-s_preparing_y;
        tile_build_piece(s_preparing_index,s_preparing_y,rows);
        uint32_t prepare_us=(uint32_t)(esp_timer_get_time()-prepare_started);
        if(prepare_us>s_max_prepare_us)s_max_prepare_us=prepare_us;
        s_preparing_y+=rows;
        if(s_preparing_y==TILE_HEIGHT)s_preparing_index=INT32_MIN;
    }
}

bool teleprompter_ui_present(void)
{
    if (!s_text[0]||s_guide_only) {
        s_guide_repaint=2;
        bool starting=s_guide_animation_pending&&s_ui_presentable;
        if(starting){
            s_guide_animation_pending=false;
            /* Flex grows the parent to fill the card. Slide only the actual
             * QR and copy bounds, avoiding hundreds of empty rows per frame. */
            lv_area_t content;lv_obj_get_coords(s_welcome_copy,&content);
            if(!lv_obj_is_hidden(s_welcome_visual)){
                lv_area_t visual;lv_obj_get_coords(s_welcome_visual,&visual);
                if(visual.x1<content.x1)content.x1=visual.x1;
                if(visual.y1<content.y1)content.y1=visual.y1;
                if(visual.x2>content.x2)content.x2=visual.x2;
                if(visual.y2>content.y2)content.y2=visual.y2;
            }
            s_guide_sliding=display_port_guide_slide_begin(&content,
                lv_obj_get_style_bg_color(s_welcome_card,0),TP_MOTION_GUIDE_SLIDE_PX);
            s_guide_slide_offset=-1;s_guide_slide_frames=0;
            s_guide_slide_last_us=0;s_guide_slide_max_gap_us=0;s_guide_slide_max_present_us=0;
            if(!s_guide_sliding)ui_motion_slide_left(s_welcome_content);
            ESP_LOGI(TAG,"GUIDE_MOTION stage=%u loading=%d dma=%d size=%dx%d",(unsigned)s_guide_tracker.stage,
                !lv_obj_is_hidden(s_welcome_loader),s_guide_sliding,lv_area_get_width(&content),lv_area_get_height(&content));
        }
        uint32_t elapsed=starting?0:lv_tick_get()-s_guide_slide_started;
        uint32_t progress=elapsed<TP_MOTION_GUIDE_SLIDE_MS?elapsed*1024/TP_MOTION_GUIDE_SLIDE_MS:1024;
        uint32_t eased=(uint64_t)progress*progress*(3072-2*progress)/(1024*1024);
        int offset=(TP_MOTION_GUIDE_SLIDE_PX*(1024-eased)+512)/1024;
        /* Let LVGL present the independent operator card without tile blits. */
        bool finishing=s_guide_sliding&&elapsed>=TP_MOTION_GUIDE_SLIDE_MS;
        if (!display_port_begin_frame(s_force_present||(s_guide_sliding&&(offset!=s_guide_slide_offset||finishing)))) return false;
        int64_t present_started=esp_timer_get_time();
        if(s_guide_sliding){display_port_guide_slide_draw(offset);s_guide_slide_offset=offset;++s_guide_slide_frames;}
        display_port_present();s_force_present=false;
        if(s_guide_sliding){
            int64_t now=esp_timer_get_time();
            uint32_t duration=now-present_started;
            if(duration>s_guide_slide_max_present_us)s_guide_slide_max_present_us=duration;
            if(s_guide_slide_last_us&&now-s_guide_slide_last_us>s_guide_slide_max_gap_us)
                s_guide_slide_max_gap_us=now-s_guide_slide_last_us;
            s_guide_slide_last_us=now;
        }
        if(starting)s_guide_slide_started=lv_tick_get();
        else if(finishing){
            s_guide_sliding=false;display_port_guide_slide_end();
            ESP_LOGI(TAG,"GUIDE_MOTION_DONE frames=%lu elapsed_ms=%lu max_gap_us=%lu present_max_us=%lu",
                (unsigned long)s_guide_slide_frames,(unsigned long)elapsed,
                (unsigned long)s_guide_slide_max_gap_us,(unsigned long)s_guide_slide_max_present_us);
        }
        return true;
    }
    if (!s_canvas_buffer) return false;
    bool presented = false;
    int y = s_text_origin - (int)s_scroll;
    s_frame_tile_count=0;
    int first_index=-1,last_index=-1;
    for(int row=0;row<s_height;){
        visible_piece_t piece=visible_piece(y,row);int index=piece.index;
        if(index>=0){
            if(first_index<0)first_index=index;
            last_index=index;
            bool found=false;for(unsigned p=0;p<s_frame_tile_count;++p)if(s_frame_tiles[p]==index){found=true;break;}
            if(!found)s_frame_tiles[s_frame_tile_count++]=index;
        }
        row+=piece.count;
    }
    /* A visible miss uses the shared canvas, so abandon any partial prefetch
     * before filling it. All visible sources are completed before DMA starts. */
    for(unsigned p=0;p<s_frame_tile_count;++p)if(!tile_find(s_frame_tiles[p])){
        s_preparing_index=INT32_MIN;s_preparing_tile=NULL;
        ++s_visible_tile_misses;tile_build(s_frame_tiles[p]);
    }
    if(s_force_present)s_guide_repaint=2;
    if (display_port_begin_frame(y != s_last_y || s_force_present)) {
        int64_t started = esp_timer_get_time();
        if(s_countdown_shown>0)for(int row=0;row<s_height;row+=TILE_HEIGHT){
            int count=s_height-row;if(count>TILE_HEIGHT)count=TILE_HEIGHT;
            blit_text_segment(s_blank_tile,row,count,0);
        }
        for(int row=0;!s_countdown_shown&&row<s_height;){
            visible_piece_t piece=visible_piece(y,row);row+=piece.count;
            script_tile_t *tile=tile_find(piece.index);
            int offset=piece.offset,count=piece.count,destination=piece.destination;
            if(s_mirror&2){destination=s_height-destination-count;offset=TILE_HEIGHT-offset-count;}
            /* Split only at the stationary reading band's boundaries. The
             * cached bright/dim glyphs use DMA copies, without per-frame blend. */
            while(count>0){
                int chunk=count;
                bool bright=!s_center_highlight || (destination>=s_band_top && destination<s_band_bottom);
                if(s_center_highlight && destination<s_band_top && destination+chunk>s_band_top)chunk=s_band_top-destination;
                else if(s_center_highlight && destination<s_band_bottom && destination+chunk>s_band_bottom)chunk=s_band_bottom-destination;
                blit_text_segment(bright||!tile->dim_pixels?tile->pixels:tile->dim_pixels,destination,chunk,offset);
                destination+=chunk;offset+=chunk;count-=chunk;
            }
        }
        if(!s_countdown_shown && s_sight_guide){
            guide_strip(s_margin_left,VIEWPORT_TOP+s_band_top,s_width,2);
            guide_strip(s_margin_left,VIEWPORT_TOP+s_band_bottom-2,s_width,2);
            guide_strip(s_margin_left,VIEWPORT_TOP+s_band_top,2,s_band_bottom-s_band_top);
            guide_strip(s_margin_left+s_width-2,VIEWPORT_TOP+s_band_top,2,s_band_bottom-s_band_top);
            if(s_guide_repaint)--s_guide_repaint;
        }
        if(s_countdown_shown>0){
            int x=s_margin_left+(s_width-COUNTDOWN_SIZE)/2;
            int cy=(s_rotation%180==0?1280:800)/2-COUNTDOWN_SIZE/2;
            int px=x,py=cy;
            if(s_rotation==90){px=cy;py=1280-x-COUNTDOWN_SIZE;}
            else if(s_rotation==270){px=800-cy-COUNTDOWN_SIZE;py=x;}
            else if(s_rotation==180){px=800-x-COUNTDOWN_SIZE;py=1280-cy-COUNTDOWN_SIZE;}
            display_port_blit(px,py,COUNTDOWN_SIZE,COUNTDOWN_SIZE,s_countdown_pixels,COUNTDOWN_SIZE,COUNTDOWN_SIZE,0,0);
        }
        /* These metrics/layout calculations only update LVGL objects, not
         * their pixels. Run the periodic work while DMA reads immutable
         * tiles, leaving the next frame's short composition window clear.
         * LVGL draws the changed bars in the following UI iteration. */
        if(s_bar_update_pending){s_bar_update_pending=false;bars_update(&s_previous);}
        /* DMA only reads the pinned visible tiles. Prepare a non-visible
         * tile while it copies this frame, then wait for scan completion. */
        prepare_tiles(first_index,last_index);
        display_port_present();
        presented = true;
        ++s_drawn_frames;
        s_last_y = y; s_force_present = false;
        uint32_t elapsed = esp_timer_get_time() - started;
        int64_t stamp=esp_timer_get_time();
        if(!s_metric_start)s_metric_start=stamp;
        if(s_metric_last && s_previous.playing && !s_countdown_shown){
            double interval=(stamp-s_metric_last)/1000.0;
            double speed=fabs((double)s_previous.speed*(s_previous.shuttle?s_previous.shuttle:1));
            double budget=fmax(1000.0/60.0,speed>0?1000.0/speed:1000.0);
            if(interval>budget*1.5+2)++s_late_frames;
            s_metric_sum+=interval;s_metric_sum2+=interval*interval;s_metric_render+=elapsed/1000.0;++s_metric_frames;
            if(stamp-s_metric_start>=2000000 && s_metric_frames){
                s_frame_ms=s_metric_sum/s_metric_frames;s_fps=s_frame_ms>0?1000.0/s_frame_ms:0;
                s_jitter_ms=sqrt(fmax(0,s_metric_sum2/s_metric_frames-s_frame_ms*s_frame_ms));
                s_render_ms=s_metric_render/s_metric_frames;
                s_metric_frames=0;s_metric_sum=s_metric_sum2=s_metric_render=0;s_metric_start=stamp;
            }
        }else{s_metric_frames=0;s_metric_sum=s_metric_sum2=s_metric_render=0;s_metric_start=stamp;s_fps=s_jitter_ms=s_render_ms=0;}
        s_metric_last=stamp;
        s_compose_us += elapsed; ++s_compose_frames;
        if (elapsed > s_max_compose_us) s_max_compose_us = elapsed;
    }
    if(!presented){
        /* Low speeds can keep the same integer pixel for several ticks.
         * Publish bar changes anyway rather than waiting for text motion. */
        if(s_bar_update_pending){s_bar_update_pending=false;bars_update(&s_previous);}
        prepare_tiles(first_index,last_index);
    }
    if (esp_timer_get_time() - s_report_us >= 10000000) {
        ESP_LOGI(TAG, "SCROLL_FRAME frames=%lu avg_us=%lu max_us=%lu tiles=%lu build_avg_us=%lu visible_misses=%lu prepare_max_us=%lu",
                 (unsigned long)s_compose_frames,
                 (unsigned long)(s_compose_frames ? s_compose_us / s_compose_frames : 0),
                 (unsigned long)s_max_compose_us, (unsigned long)s_built_tiles,
                 (unsigned long)(s_built_tiles ? s_build_us / s_built_tiles : 0),
                 (unsigned long)s_visible_tile_misses,(unsigned long)s_max_prepare_us);
        s_visible_tile_misses=s_max_prepare_us=0;
        s_compose_frames = s_built_tiles = s_max_compose_us = 0; s_compose_us = s_build_us = 0;
        s_report_us = esp_timer_get_time();
    }
    return presented;
}

static void layout(const prompter_status_t *state)
{
    lv_display_t *display = display_port_get_lvgl_display();
    lv_display_set_rotation(display, (lv_display_rotation_t)(state->rotation / 90));
    s_rotation=state->rotation;
    int width = lv_display_get_horizontal_resolution(display);
    int height = lv_display_get_vertical_resolution(display);
    s_margin_left=state->margin_left;
    /* Independent chrome margins; the reading anchor is the physical center. */
    s_viewport_top=(s_bars.header?s_bars.header_height:0)+8;
    int bottom=(s_bars.footer?s_bars.footer_height:0)+8;
    s_width=width-state->margin_left-state->margin_right;s_height=height-s_viewport_top-bottom;
    tight_metrics(state->font_size,s_text);
    /* A reading band surrounds one complete font line. Extra line spacing
     * belongs between lines and must not change the band's anchor or size. */
    int band_height=s_font->line_height+32;
    if(band_height>s_height-8)band_height=s_height-8;
    int center=height/2-s_viewport_top;
    s_band_top=center-band_height/2;
    s_band_bottom=s_band_top+band_height;
    /* Reserve the reading band as the leading blank line. Mirroring applies
     * to the complete reading space, including this initial lead-in. */
    s_text_origin=((state->mirror&2)?s_height-center:center)+band_height/2+state->line_spacing;
    s_sight_guide=state->sight_guide;
    lv_obj_set_pos(s_viewport,s_margin_left,VIEWPORT_TOP);
    lv_obj_set_size(s_viewport,s_width,s_height);
    lv_obj_set_hidden(s_reading_marker,true);
    lv_obj_set_width(s_script_label,s_width);
    lv_obj_set_style_text_font(s_script_label, s_font, 0);
    lv_obj_set_style_text_letter_space(s_script_label, state->letter_spacing, 0);
    lv_obj_set_style_text_line_space(s_script_label, state->line_spacing, 0);
    lv_obj_set_style_text_align(s_script_label,state->text_align==1?LV_TEXT_ALIGN_CENTER:state->text_align==2?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT,0);
    /* A size change reuses the font pointer: explicitly invalidate text metrics. */
    lv_obj_refresh_style(s_script_label, LV_PART_MAIN, LV_STYLE_TEXT_FONT);
    int header=s_bars.header_height,footer=s_bars.footer_height;
    int header_size=s_bars.header_font_size,footer_size=s_bars.footer_font_size;
    if(header_size>header-8)header_size=header-8;
    if(footer_size>footer-8)footer_size=footer-8;
    lv_tiny_ttf_set_size(s_info_font,header_size);lv_tiny_ttf_set_size(s_bar_font,footer_size);
    /* TTF line boxes can exceed nominal pixels: shrink to the available bar. */
    while(s_info_font->line_height>header-8&&header_size>8)lv_tiny_ttf_set_size(s_info_font,--header_size);
    while(s_bar_font->line_height>footer-8&&footer_size>8)lv_tiny_ttf_set_size(s_bar_font,--footer_size);
    s_footer_font_size=footer_size;
    s_footer_icon_size=footer_size*3/4;if(s_footer_icon_size<8)s_footer_icon_size=8;
    lv_obj_t *header_labels[]={s_title,s_header_left,s_header_right,s_timecode};
    for(unsigned i=0;i<4;i++){
        lv_obj_set_style_text_font(header_labels[i],s_info_font,0);
        lv_obj_refresh_style(header_labels[i],LV_PART_MAIN,LV_STYLE_TEXT_FONT);
    }
    lv_obj_refresh_style(s_footer_right,LV_PART_MAIN,LV_STYLE_TEXT_FONT);
    int header_y=(header-s_info_font->line_height)/2;
    lv_obj_set_size(s_title,width/4,s_info_font->line_height);
    lv_obj_align(s_title,LV_ALIGN_TOP_MID,0,header_y);
    int group_width=width*3/8-16;
    lv_obj_set_size(s_header_left,group_width,s_info_font->line_height);
    lv_obj_align(s_header_left,LV_ALIGN_TOP_LEFT,16,header_y);
    int icon=header_size;
    lv_obj_set_size(s_header_right,group_width-icon-8,s_info_font->line_height);
    lv_obj_align(s_header_right,LV_ALIGN_TOP_RIGHT,-16,header_y);
    lv_obj_set_size(s_state_icon,icon,icon);
    lv_obj_align(s_state_icon,LV_ALIGN_TOP_RIGHT,-16-group_width+icon,(header-icon)/2);
    int dot=icon*3/5;if(dot<8)dot=8;
    lv_obj_set_size(s_play_dot,dot,dot);
    lv_obj_set_pos(s_play_dot,(icon-dot)/2,(icon-dot)/2);
    for(int i=0;i<2;i++){lv_obj_set_size(s_pause_icon[i],3,icon-2);lv_obj_set_pos(s_pause_icon[i],2+i*(icon-6),1);}
    lv_obj_set_size(s_stop_icon,icon-4,icon-4);lv_obj_set_pos(s_stop_icon,2,2);
    lv_obj_set_size(s_footer_right,width-32,footer);
    lv_obj_align(s_footer_right,LV_ALIGN_BOTTOM_LEFT,16,0);
    bool landscape=width>height;
    lv_obj_set_width(s_welcome_card,landscape?width-120:width-80);
    lv_obj_set_height(s_welcome_card,landscape?TP_GUIDE_LANDSCAPE_HEIGHT:TP_GUIDE_PORTRAIT_HEIGHT);
    lv_obj_set_style_pad_all(s_welcome_card,landscape?32:28,0);
    lv_obj_set_flex_flow(s_welcome_content,landscape?LV_FLEX_FLOW_ROW:LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_column(s_welcome_content,TP_GUIDE_COLUMN_GAP,0);
    lv_obj_set_style_pad_row(s_welcome_content,28,0);
    welcome_content_layout(landscape);
    for(unsigned i=0;i<TP_GUIDE_STEP_COUNT;i++)lv_obj_set_width(lv_obj_get_parent(s_step_dot[i]),lv_pct(100/TP_GUIDE_STEP_COUNT));
    lv_obj_update_layout(s_welcome_card);
    lv_obj_center(s_welcome_card);
    lv_obj_invalidate(lv_screen_active());
    /* A 180-degree change keeps the logical size. Force every layer through
     * the new physical mapping even when LVGL has no size change to redraw. */
    if (!s_layout_ready || state->rotation != s_previous.rotation) {
        lv_obj_invalidate(lv_layer_top());
        lv_obj_invalidate(lv_layer_sys());
        lv_refr_now(display);
    }
    s_force_present = true;
    ESP_LOGI(TAG, "UI_LAYOUT %dx%d rotation=%d native_font=%d",
             width, height, state->rotation, state->font_size);
    s_layout_ready = true;
}

esp_err_t teleprompter_ui_init(void)
{
    if (display_port_get_lvgl_display() == NULL) {
        ESP_LOGW(TAG, "UI deferred until a display panel is configured");
        return ESP_OK;
    }

    lv_obj_t *screen = lv_screen_active();
    /* Absolute-positioned screen content must never create LVGL scrollbars. */
    lv_obj_set_scrollable(screen,false);
    lv_obj_set_scrollbar_mode(screen,LV_SCROLLBAR_MODE_OFF);
    s_font_source = lv_tiny_ttf_create_data_ex(font_start, font_end - font_start, 48,
                                       LV_FONT_KERNING_NONE, 512);
    s_countdown_font = lv_tiny_ttf_create_data_ex(font_start, font_end - font_start, 160,
                                                 LV_FONT_KERNING_NONE, 16);
    s_info_font=lv_tiny_ttf_create_data_ex(font_start,font_end-font_start,30,LV_FONT_KERNING_NONE,128);
    s_bar_font=lv_tiny_ttf_create_data_ex(font_start,font_end-font_start,22,LV_FONT_KERNING_NONE,128);
    s_ready_font=lv_tiny_ttf_create_data_ex(font_start,font_end-font_start,32,LV_FONT_KERNING_NONE,16);
    s_guide_font=lv_tiny_ttf_create_data_ex(font_start,font_end-font_start,24,LV_FONT_KERNING_NONE,128);
    s_step_font=lv_tiny_ttf_create_data_ex(font_start,font_end-font_start,22,LV_FONT_KERNING_NONE,64);
    s_font=&s_tight_font;
    if (!s_font_source || !s_countdown_font || !s_info_font || !s_ready_font || !s_bar_font || !s_guide_font || !s_step_font) return ESP_ERR_NO_MEM;
    s_guide_pixels=heap_caps_aligned_alloc(64,1280*4*sizeof(uint16_t),MALLOC_CAP_SPIRAM);
    s_blank_tile=heap_caps_aligned_calloc(64,1280*TILE_HEIGHT,sizeof(uint16_t),MALLOC_CAP_SPIRAM);
    if(!s_guide_pixels || !s_blank_tile)return ESP_ERR_NO_MEM;
    ESP_ERROR_CHECK(esp_cache_msync(s_blank_tile,1280*TILE_HEIGHT*2,ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    uint16_t guide_color=lv_color_to_u16(lv_color_hex(0xcbef9c));
    for(int i=0;i<1280*4;++i)s_guide_pixels[i]=guide_color;
    ESP_ERROR_CHECK(esp_cache_msync(s_guide_pixels,1280*4*sizeof(uint16_t),ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    s_title = lv_label_create(screen);
    lv_label_set_text(s_title, "TELEPROMPTER");
    lv_obj_set_style_text_font(s_title, s_info_font, 0);
    lv_obj_set_style_text_color(s_title, lv_color_hex(0x8aa4b8), 0);
    lv_obj_set_style_text_align(s_title,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(s_title,LV_LABEL_LONG_MODE_DOTS);

    s_reading_marker = lv_obj_create(screen);
    lv_obj_remove_style_all(s_reading_marker);
    lv_obj_set_size(s_reading_marker, 3, 40);
    lv_obj_set_style_bg_color(s_reading_marker, lv_color_hex(0xcbef9c), 0);
    lv_obj_set_style_bg_opa(s_reading_marker, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(s_reading_marker,false);

    s_viewport = lv_obj_create(screen);
    lv_obj_remove_style_all(s_viewport);
    lv_obj_set_scrollable(s_viewport,false);
    s_script_label = lv_label_create(s_viewport);
    lv_obj_set_hidden(s_script_label, true);
    s_canvas = lv_canvas_create(screen);
    lv_obj_set_hidden(s_canvas, true);
    lv_obj_set_style_text_align(s_script_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_line_space(s_script_label, 12, 0);
    lv_obj_set_style_text_color(s_script_label, lv_color_white(), 0);
    s_footer = lv_label_create(screen);
    lv_obj_set_style_text_font(s_footer, s_bar_font, 0);
    lv_obj_set_style_text_color(s_footer, lv_color_hex(0x8aa4b8), 0);
    lv_obj_set_style_text_align(s_footer,LV_TEXT_ALIGN_LEFT,0);
    lv_label_set_long_mode(s_footer,LV_LABEL_LONG_MODE_DOTS);
    s_header_left=lv_label_create(screen);s_header_right=lv_label_create(screen);s_timecode=lv_label_create(screen);
    s_footer_right=lv_obj_create(screen);lv_obj_remove_style_all(s_footer_right);
    lv_obj_set_scrollable(s_footer_right,false);lv_obj_set_scrollbar_mode(s_footer_right,LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_hidden(s_timecode,true);
    s_state_icon=lv_obj_create(screen);lv_obj_remove_style_all(s_state_icon);
    lv_obj_set_scrollable(s_state_icon,false);
    s_play_dot=lv_obj_create(s_state_icon);lv_obj_remove_style_all(s_play_dot);
    lv_obj_set_style_radius(s_play_dot,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(s_play_dot,lv_color_hex(0xef4444),0);
    lv_obj_set_style_bg_opa(s_play_dot,LV_OPA_COVER,0);
    lv_obj_set_clickable(s_play_dot,false);lv_obj_set_scrollable(s_play_dot,false);
    s_stop_icon=lv_obj_create(s_state_icon);lv_obj_remove_style_all(s_stop_icon);
    lv_obj_set_style_bg_color(s_stop_icon,lv_color_hex(0x8aa4b8),0);lv_obj_set_style_bg_opa(s_stop_icon,LV_OPA_COVER,0);
    lv_obj_set_clickable(s_stop_icon,false);lv_obj_set_scrollable(s_stop_icon,false);
    for(int i=0;i<2;i++){
        s_pause_icon[i]=lv_obj_create(s_state_icon);lv_obj_remove_style_all(s_pause_icon[i]);
        lv_obj_set_style_bg_color(s_pause_icon[i],lv_color_hex(0x8aa4b8),0);lv_obj_set_style_bg_opa(s_pause_icon[i],LV_OPA_COVER,0);
    }
    lv_obj_t *info_labels[]={s_header_left,s_header_right,s_timecode};
    for(int i=0;i<3;i++){
        lv_obj_set_style_text_font(info_labels[i],s_info_font,0);
        lv_obj_set_style_text_color(info_labels[i],lv_color_hex(0x8aa4b8),0);
        lv_obj_set_style_text_align(info_labels[i],i>=1?LV_TEXT_ALIGN_RIGHT:LV_TEXT_ALIGN_LEFT,0);
        lv_label_set_long_mode(info_labels[i],LV_LABEL_LONG_MODE_DOTS);
    }
    s_welcome_card=lv_obj_create(screen);
    lv_obj_set_scrollable(s_welcome_card,false);
    lv_obj_set_height(s_welcome_card,LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_welcome_card,lv_color_hex(0x11191e),0);
    lv_obj_set_style_bg_opa(s_welcome_card,LV_OPA_COVER,0);
    lv_obj_set_style_border_color(s_welcome_card,lv_color_hex(0x293a44),0);
    lv_obj_set_style_border_width(s_welcome_card,1,0);
    lv_obj_set_style_radius(s_welcome_card,20,0);
    lv_obj_set_style_pad_all(s_welcome_card,28,0);
    lv_obj_set_style_pad_row(s_welcome_card,28,0);
    lv_obj_set_flex_flow(s_welcome_card,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_welcome_card,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    s_welcome_steps=lv_obj_create(s_welcome_card);
    lv_obj_remove_style_all(s_welcome_steps);lv_obj_set_width(s_welcome_steps,lv_pct(100));
    lv_obj_set_height(s_welcome_steps,60);lv_obj_set_scrollable(s_welcome_steps,false);
    lv_obj_set_flex_flow(s_welcome_steps,LV_FLEX_FLOW_ROW);
    for(unsigned i=0;i<TP_GUIDE_STEP_COUNT;i++){
        lv_obj_t *item=lv_obj_create(s_welcome_steps);lv_obj_remove_style_all(item);
        lv_obj_set_size(item,lv_pct(100/TP_GUIDE_STEP_COUNT),60);lv_obj_set_scrollable(item,false);
        lv_obj_set_flex_flow(item,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(item,10,0);
        lv_obj_set_flex_align(item,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
        s_step_dot[i]=lv_obj_create(item);lv_obj_remove_style_all(s_step_dot[i]);
        lv_obj_set_size(s_step_dot[i],38,38);lv_obj_set_style_radius(s_step_dot[i],LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_opa(s_step_dot[i],LV_OPA_COVER,0);lv_obj_set_scrollable(s_step_dot[i],false);
        s_step_number[i]=lv_label_create(s_step_dot[i]);
        lv_obj_set_style_text_font(s_step_number[i],s_step_font,0);
        lv_label_set_text_fmt(s_step_number[i],"%u",i+1);lv_obj_center(s_step_number[i]);
        s_step_label[i]=lv_label_create(item);lv_obj_set_style_text_font(s_step_label[i],s_step_font,0);
    }
    s_welcome_content=lv_obj_create(s_welcome_card);lv_obj_remove_style_all(s_welcome_content);
    lv_obj_set_width(s_welcome_content,lv_pct(100));lv_obj_set_height(s_welcome_content,LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(s_welcome_content,1);
    lv_obj_set_scrollable(s_welcome_content,false);
    lv_obj_set_flex_align(s_welcome_content,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    s_welcome_visual=lv_obj_create(s_welcome_content);lv_obj_remove_style_all(s_welcome_visual);
    lv_obj_set_size(s_welcome_visual,TP_GUIDE_QR_SIZE,TP_GUIDE_QR_SIZE);lv_obj_set_scrollable(s_welcome_visual,false);
    lv_obj_set_style_bg_opa(s_welcome_visual,LV_OPA_COVER,0);lv_obj_set_style_bg_color(s_welcome_visual,lv_color_white(),0);
    lv_obj_set_style_radius(s_welcome_visual,12,0);
    s_welcome_qr=lv_qrcode_create(s_welcome_visual);
    lv_qrcode_set_size(s_welcome_qr,TP_GUIDE_QR_SIZE-2*TP_GUIDE_QR_PADDING);lv_obj_center(s_welcome_qr);
    lv_qrcode_set_dark_color(s_welcome_qr,lv_color_black());
    lv_qrcode_set_light_color(s_welcome_qr,lv_color_white());
    lv_qrcode_set_quiet_zone(s_welcome_qr,true);
    s_welcome_copy=lv_obj_create(s_welcome_content);lv_obj_remove_style_all(s_welcome_copy);
    lv_obj_set_height(s_welcome_copy,LV_SIZE_CONTENT);lv_obj_set_scrollable(s_welcome_copy,false);
    lv_obj_set_flex_flow(s_welcome_copy,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_welcome_copy,TP_GUIDE_TITLE_GAP,0);
    s_welcome_title_row=lv_obj_create(s_welcome_copy);lv_obj_remove_style_all(s_welcome_title_row);
    lv_obj_set_size(s_welcome_title_row,lv_pct(100),TP_GUIDE_TITLE_LINE_HEIGHT);
    lv_obj_set_flex_flow(s_welcome_title_row,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(s_welcome_title_row,14,0);
    lv_obj_set_scrollable(s_welcome_title_row,false);
    s_welcome_loader=lv_arc_create(s_welcome_title_row);lv_obj_set_size(s_welcome_loader,32,32);
    lv_arc_set_bg_angles(s_welcome_loader,0,360);lv_arc_set_angles(s_welcome_loader,0,90);
    lv_obj_set_style_arc_width(s_welcome_loader,3,LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_welcome_loader,3,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_welcome_loader,lv_color_hex(0x293a44),LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_welcome_loader,lv_color_hex(0x8fcee9),LV_PART_INDICATOR);
    lv_obj_remove_style(s_welcome_loader,NULL,LV_PART_KNOB);lv_obj_set_clickable(s_welcome_loader,false);
    lv_obj_set_hidden(s_welcome_loader,true);
    s_welcome_description=lv_obj_create(s_welcome_copy);lv_obj_remove_style_all(s_welcome_description);
    lv_obj_set_size(s_welcome_description,lv_pct(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_welcome_description,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_welcome_description,TP_GUIDE_PARAGRAPH_GAP,0);
    lv_obj_set_scrollable(s_welcome_description,false);
    s_welcome_title=lv_label_create(s_welcome_title_row);
    s_welcome_body=lv_label_create(s_welcome_description);s_welcome_body_more=lv_label_create(s_welcome_description);
    lv_obj_t *labels[]={s_welcome_title,s_welcome_body,s_welcome_body_more};
    for(int i=0;i<3;++i){
        lv_obj_set_width(labels[i],i==0?LV_SIZE_CONTENT:lv_pct(100));
        lv_label_set_long_mode(labels[i],LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(labels[i],i==0?s_ready_font:s_guide_font,0);
        lv_obj_set_style_text_color(labels[i],lv_color_hex(i==0?0xeaf0f1:0xaab8c0),0);
        lv_obj_set_style_text_line_space(labels[i],i==0?0:TP_GUIDE_BODY_LINE_HEIGHT-s_guide_font->line_height,0);
        lv_obj_set_style_text_letter_space(labels[i],0,0);
    }
    s_countdown_canvas=lv_canvas_create(screen);lv_obj_set_hidden(s_countdown_canvas,true);
    s_countdown_buffer=lv_draw_buf_create(COUNTDOWN_SIZE,COUNTDOWN_SIZE,LV_COLOR_FORMAT_RGB565,LV_STRIDE_AUTO);
    s_countdown_pixels=heap_caps_aligned_alloc(64,COUNTDOWN_SIZE*COUNTDOWN_SIZE*2,MALLOC_CAP_SPIRAM);
    if(!s_countdown_buffer || !s_countdown_pixels)return ESP_ERR_NO_MEM;
    lv_canvas_set_draw_buf(s_countdown_canvas,s_countdown_buffer);
    teleprompter_ui_tick();
    ESP_LOGI(TAG, "Teleprompter LVGL UI initialized");
    return ESP_OK;
}

void teleprompter_ui_set_text(const char *text)
{
    if (text) prompter_set_script(text, strlen(text));
}

void teleprompter_ui_tick(void)
{
    if (!s_script_label) return;
    prompter_status_t state;
    prompter_snapshot(&state, NULL);
    const bool guide_changed=s_guide_changed;
    int language;bool show_info;
    device_settings_ui(&language,&show_info);
    device_bars_t bars;device_settings_bars(&bars);
    /* Per-document playback visibility wins over device defaults. */
    if(state.length && !s_guide_only && state.show_info>=0)bars.header=bars.footer=state.show_info!=0;
    bool bars_changed=memcmp(&bars,&s_bars,sizeof(bars))!=0;
    bool bars_geometry=bars.header!=s_bars.header||bars.footer!=s_bars.footer||
        bars.header_height!=s_bars.header_height||bars.footer_height!=s_bars.footer_height||
        bars.header_font_size!=s_bars.header_font_size||bars.footer_font_size!=s_bars.footer_font_size;
    s_bars=bars;
    bool language_changed=language!=s_language;
    bool info_changed=show_info!=s_show_info;
    if(language_changed){s_language=language;s_welcome[0]=0;lv_label_set_text(s_title,s_language?"TELEPROMPTER":"提词器");}
    bool text_changed = state.script_revision != s_previous.script_revision;
    if (text_changed) prompter_snapshot(&state, s_text);
    if(info_changed || bars_changed || language_changed || text_changed || s_guide_changed || !s_layout_ready){
        s_show_info=show_info;
        bool guide=!s_text[0]||s_guide_only;
        lv_obj_set_hidden(s_title,!guide&&!bars.header);
        lv_obj_set_hidden(s_header_left,!guide&&!bars.header);lv_obj_set_hidden(s_header_right,!guide&&!bars.header);
        lv_obj_set_hidden(s_footer,true);lv_obj_set_hidden(s_footer_right,guide||!bars.footer);
        lv_obj_set_hidden(s_state_icon,guide||!bars.header||!(bars.fields&TP_BAR_STATE));
        s_force_present=true;
    }
    if (text_changed || s_guide_changed || !s_layout_ready) {
        lv_obj_set_hidden(s_welcome_card,s_text[0]!=0&&!s_guide_only);
        if(s_text[0]&&!s_guide_only){
            s_guide_sliding=false;display_port_guide_slide_end();
            ui_motion_stop(s_welcome_content);
            ui_motion_stop(s_welcome_loader);
        }else{
            s_guide_animation_pending=true;
            if(tp_guide_copy[s_guide_tracker.stage].loading)ui_motion_spin(s_welcome_loader);
        }
        lv_obj_invalidate(lv_screen_active());
        s_force_present=true;
        s_guide_changed=false;
    }
    prompter_status_t drawing=state;
    if(!state.length||s_guide_only)drawing.rotation=90;
    bool layout_changed = !s_layout_ready || bars_geometry || guide_changed || state.font_size != s_previous.font_size ||
                          drawing.rotation != s_rotation ||
                          state.letter_spacing != s_previous.letter_spacing ||
                          state.line_spacing != s_previous.line_spacing ||
                          state.margin_left != s_previous.margin_left || state.margin_right != s_previous.margin_right ||
                          state.text_align != s_previous.text_align || state.mirror != s_previous.mirror;
    bool cache_changed=layout_changed || text_changed || state.loop!=s_previous.loop || state.text_color!=s_previous.text_color ||
                       state.bold!=s_previous.bold;
    if(state.sight_guide!=s_previous.sight_guide){s_sight_guide=state.sight_guide;s_force_present=true;}
    if(state.loop!=s_previous.loop)s_force_present=true;
    if (text_changed) layout_changed=true;
    if (layout_changed) layout(&drawing);
    if (text_changed) {
        lv_label_set_text_static(s_script_label, display_text());
    }
    if (layout_changed || state.script_revision != s_previous.script_revision)
        lv_obj_update_layout(s_script_label);
    bool guide_visible=!s_text[0]||s_guide_only;
    if(cache_changed&&guide_visible)s_reader_cache_pending=true;
    if (!guide_visible&&(cache_changed||s_reader_cache_pending)) {
        /* Switching to the landscape standby guide is presentation only.
         * Preserve reading progress while its temporary layout is built,
         * and restore it when the original reading page returns. */
        float position=(!text_changed&&(!layout_changed||bars_geometry||guide_changed))?s_scroll:0;
        cache_reset(&drawing,layout_changed || text_changed || s_reader_cache_pending,position);
        s_reader_cache_pending=false;
    }
    if(!guide_visible&&!cache_changed&&state.center_highlight!=s_previous.center_highlight)highlight_update(state.center_highlight);
    if (state.rewind_revision != s_previous.rewind_revision ||
        state.script_revision != s_previous.script_revision) s_scroll = 0;
    int countdown = (state.countdown_remaining_ms + 999) / 1000;
    if (countdown != s_countdown_shown) {
        if(!s_countdown_shown)s_countdown_digit=-1;
        s_countdown_shown = countdown;
        s_force_present = true;
    }
    if(countdown && (lv_tick_get()-s_countdown_frame>=50 || cache_changed))countdown_build(&state,lv_tick_get());
    char address[16];
    wifi_settings_address(address, sizeof(address));
    // Portal choices can change independently of playback/network revisions.
    if(!s_text[0]||s_guide_only)welcome_update(address);
    if (state.revision != s_previous.revision || strcmp(address, s_address) || language_changed || info_changed) {
        memcpy(s_address, address, sizeof(s_address));
        welcome_update(address);
    }
    if(lv_tick_get()-s_bar_tick>=1000||bars_changed||language_changed||guide_changed||text_changed||state.playing!=s_previous.playing){
        s_bar_tick=lv_tick_get();
        s_bar_update_pending=state.playing&&s_previous.playing&&!layout_changed&&!guide_visible&&!bars_changed&&!language_changed&&!text_changed;
        if(!s_bar_update_pending)bars_update(&state);
    }
    uint32_t now = lv_tick_get();
    uint32_t elapsed = s_tick ? now - s_tick : 0;
    s_tick = now;
    if (state.playing && s_previous.playing) {
        s_scroll += (float)state.speed * (state.shuttle ? state.shuttle : 1) * elapsed / 1000.0f;
        if (s_scroll < 0) {
            s_scroll = 0;
            if(prompter_playback_stopped(state.script_revision,state.rewind_revision)){
                prompter_snapshot(&state,NULL);
                s_force_present=true;bars_update(&state);
            }
        }
        float end=state.loop?s_loop_period:s_end_scroll;
        if (state.playing && s_scroll >= end) {
            bool repeat=prompter_playback_finished(state.script_revision,state.rewind_revision);
            if(repeat){s_scroll=fmodf(s_scroll,s_loop_period);++state.loop_count;}
            else {
                prompter_status_t stopped;
                prompter_snapshot(&stopped,NULL);
                /* A concurrent select/seek/pause invalidates this frame's end event. */
                if(stopped.script_revision==state.script_revision&&stopped.rewind_revision==state.rewind_revision+1&&stopped.stopped&&stopped.playback_finished){
                    state=stopped;s_scroll=0;s_force_present=true;bars_update(&state);
                    if(state.auto_next&&!state.loop)script_library_advance(state.script_revision);
                }
            }
        }
    }
    if(s_play_dot_active){
        bool bright=(lv_tick_get()-s_play_dot_started)%1200<600;
        if(bright!=s_play_dot_bright){
            s_play_dot_bright=bright;
            lv_obj_set_style_bg_color(s_play_dot,lv_color_hex(bright?0xef4444:0x782222),0);
        }
    }
    s_previous = state;
    prompter_preview_position((int)s_scroll,s_preview_revision);
    /* Position the group before its first draw; start its clock only after
     * presentation, and only after the startup overlay has been removed. */
    if(s_guide_animation_pending&&guide_visible){
        ui_motion_stop(s_welcome_content);
    }
}
