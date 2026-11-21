#include "startup_screen.h"
#include <string.h>
#include "display_port.h"
#include "prompter_state.h"
#include "device_settings.h"
#include "firmware_update.h"
#include "lvgl.h"
#include "ui_motion.h"
#include "ui_system_settings.h"

extern const uint8_t font_start[] asm("_binary_noto_sc_subset_otf_start");
extern const uint8_t font_end[] asm("_binary_noto_sc_subset_otf_end");
static lv_font_t *s_font;
static lv_font_t *s_aux_font;
static lv_obj_t *s_status, *s_progress, *s_percent;
static lv_obj_t *s_notice;
static lv_obj_t *s_startup_overlay,*s_startup_content,*s_notice_content;
static lv_font_t *s_notice_font;
static lv_font_t *s_notice_aux_font;
static int s_language;
static int s_startup_progress;
static lv_obj_t *s_update_bar, *s_update_label, *s_update_percent;
static unsigned s_update_stage, s_update_value;

bool startup_screen_tick(void)
{
    /* Startup/update/exit bypass the reader loop; they still need to execute
     * LVGL animations and publish changed chrome through the same DMA path. */
    lv_timer_handler();
    bool presented=display_port_begin_frame(false);
    if(presented)display_port_present();
    display_port_tick();
    return presented;
}

static lv_obj_t *content_create(lv_obj_t *parent)
{
    lv_obj_t *content=lv_obj_create(parent);lv_obj_remove_style_all(content);
    lv_obj_set_size(content,lv_pct(TP_SYSTEM_CONTENT_WIDTH),TP_SYSTEM_CONTENT_HEIGHT);
    lv_obj_center(content);
    lv_obj_set_scrollable(content,false);
    return content;
}

static void label_style(lv_obj_t *label, const lv_font_t *font, uint32_t color, int y)
{
    lv_obj_set_style_text_font(label,font,0);
    lv_obj_set_style_text_color(label,lv_color_hex(color),0);
    lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_width(label,lv_pct(100));
    lv_obj_align(label,LV_ALIGN_CENTER,0,y);
}

static void status_icon_create(lv_obj_t *parent)
{
    /* Fixed quarter arc: only the angle changes, never its size or center. */
    lv_obj_t *icon=lv_arc_create(parent);
    lv_obj_remove_style_all(icon);
    lv_obj_set_clickable(icon,false);
    lv_obj_set_size(icon,TP_SYSTEM_ICON_SIZE,TP_SYSTEM_ICON_SIZE);
    lv_obj_set_style_opa(icon,LV_OPA_TRANSP,LV_PART_KNOB);
    lv_obj_set_style_arc_width(icon,3,LV_PART_MAIN);
    lv_obj_set_style_arc_width(icon,3,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(icon,lv_color_hex(0x263442),LV_PART_MAIN);
    lv_obj_set_style_arc_color(icon,lv_color_hex(0x8fcee9),LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(icon,true,LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(icon,true,LV_PART_INDICATOR);
    lv_arc_set_bg_angles(icon,0,360);
    lv_arc_set_angles(icon,0,90);
    ui_motion_spin(icon);
}

static lv_obj_t *status_row_create(lv_obj_t *parent,const lv_font_t *font,lv_obj_t **label)
{
    lv_obj_update_layout(parent);
    lv_obj_t *row=lv_obj_create(parent);lv_obj_remove_style_all(row);
    lv_obj_set_size(row,lv_pct(100),LV_SIZE_CONTENT);
    lv_obj_set_scrollable(row,false);
    lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row,TP_SYSTEM_ICON_GAP,0);
    lv_obj_align(row,LV_ALIGN_CENTER,0,TP_SYSTEM_TITLE_Y);
    status_icon_create(row);
    *label=lv_label_create(row);
    label_style(*label,font,0xf3f6fa,0);
    lv_obj_set_width(*label,LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(*label,lv_obj_get_content_width(parent)-TP_SYSTEM_ICON_SIZE-TP_SYSTEM_ICON_GAP,0);
    lv_label_set_long_mode(*label,LV_LABEL_LONG_WRAP);
    return row;
}

esp_err_t startup_screen_init(void)
{
    s_startup_progress=0;
    device_settings_ui(&s_language,NULL);
    lv_display_t *display = display_port_get_lvgl_display();
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
    s_font = lv_tiny_ttf_create_data_ex(font_start, font_end-font_start, TP_SYSTEM_TITLE_FONT,
                                      LV_FONT_KERNING_NONE, 96);
    if (!s_font) return ESP_ERR_NO_MEM;
    s_aux_font=lv_tiny_ttf_create_data_ex(font_start,font_end-font_start,TP_SYSTEM_DETAIL_FONT,
                                        LV_FONT_KERNING_NONE,64);
    if(!s_aux_font){lv_tiny_ttf_destroy(s_font);s_font=NULL;return ESP_ERR_NO_MEM;}
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0b1118), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    /* Keep boot status above the reading UI while it is being constructed.
     * Clearing the active screen here used to remove the final boot step. */
    s_startup_overlay=lv_obj_create(lv_layer_top());lv_obj_remove_style_all(s_startup_overlay);
    lv_obj_set_size(s_startup_overlay,lv_pct(100),lv_pct(100));
    lv_obj_set_style_bg_color(s_startup_overlay,lv_color_hex(0x0b1118),0);
    lv_obj_set_style_bg_opa(s_startup_overlay,LV_OPA_COVER,0);
    lv_obj_set_scrollable(s_startup_overlay,false);
    s_startup_content=content_create(s_startup_overlay);
    status_row_create(s_startup_content,s_font,&s_status);
    s_progress = lv_bar_create(s_startup_content);
    lv_obj_set_size(s_progress,lv_pct(78),8);
    lv_obj_align(s_progress, LV_ALIGN_CENTER, 0, TP_SYSTEM_PROGRESS_Y);
    lv_obj_set_style_bg_color(s_progress, lv_color_hex(0x263442), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_progress, lv_color_hex(0xcbef9c), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_progress, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(s_progress, 5, LV_PART_INDICATOR);
    s_percent = lv_label_create(s_startup_content);
    label_style(s_percent,s_aux_font,0x8aa4b8,TP_SYSTEM_PERCENT_Y);
    startup_screen_update(30, "屏幕已启动", "Preparing your reading space");
    ui_motion_enter(s_startup_content,0);
    return ESP_OK;
}

void startup_screen_update(int progress, const char *status, const char *detail)
{
    if(progress<s_startup_progress)progress=s_startup_progress;
    s_startup_progress=progress;
    const char *text=s_language?detail:status;
    bool changed=strcmp(lv_label_get_text(s_status),text)!=0;
    if(changed)lv_label_set_text(s_status,text);
    ui_motion_progress(s_progress,(unsigned)progress);
    lv_label_set_text_fmt(s_percent, "%d%%", progress);
    lv_obj_align(s_percent,LV_ALIGN_CENTER,0,TP_SYSTEM_PERCENT_Y);
    if(progress>=95){
        /* This is the last update before font/widget construction. Publish it
         * at full opacity now, rather than beginning a fade at opacity zero. */
        ui_motion_stop(s_status);
        lv_refr_now(display_port_get_lvgl_display());
    }else if(changed)ui_motion_enter(s_status,0);
    startup_screen_tick();
}

void startup_screen_finish(void)
{
    if(s_startup_overlay)lv_obj_delete(s_startup_overlay);
    s_startup_overlay=NULL;
    lv_tiny_ttf_destroy(s_font);
    lv_tiny_ttf_destroy(s_aux_font);
    s_font = NULL;
    s_aux_font=NULL;
    s_startup_content=NULL;
}

void startup_screen_notice(const char *message, const char *detail)
{
    device_settings_ui(&s_language,NULL);
    if (s_notice) startup_screen_dismiss_notice();
    s_notice_font = lv_tiny_ttf_create_data_ex(font_start, font_end-font_start, TP_SYSTEM_TITLE_FONT,
                                              LV_FONT_KERNING_NONE, 64);
    s_notice_aux_font=lv_tiny_ttf_create_data_ex(font_start,font_end-font_start,TP_SYSTEM_DETAIL_FONT,
                                               LV_FONT_KERNING_NONE,64);
    s_notice = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_notice);
    lv_obj_set_size(s_notice, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_notice, lv_color_hex(0x0b1118), 0);
    lv_obj_set_style_bg_opa(s_notice, LV_OPA_COVER, 0);
    s_notice_content=content_create(s_notice);
    const lv_font_t *aux=s_notice_aux_font?s_notice_aux_font:&lv_font_montserrat_32;
    lv_obj_t *label;
    status_row_create(s_notice_content,s_notice_font?s_notice_font:&lv_font_montserrat_32,&label);
    const char *english=strstr(message,"关机")?"Shutting down":strstr(message,"开机")?"Starting up":strstr(message,"重新")?"Restarting":strstr(message,"更新")?"Updating":strstr(message,"已休眠")?"Screen asleep":"Going to sleep";
    lv_label_set_text(label, s_language?english:message);
    lv_obj_t *hint = lv_label_create(s_notice_content);
    lv_label_set_text(hint, s_language?detail:"");
    label_style(hint,aux,0x8aa4b8,TP_SYSTEM_DETAIL_Y);
    ui_motion_enter(s_notice_content,0);
    startup_screen_tick();
}

void startup_screen_exit_notice(void)
{
    if(s_notice_content)ui_motion_exit(s_notice_content);
}

void startup_screen_crash(void)
{
    startup_screen_notice("设备异常重启","Unexpected restart");
    /* A recovery message is a fixed warning, not an endless loading state. */
    lv_obj_clean(s_notice_content);
    const lv_font_t *title=s_notice_font?s_notice_font:&lv_font_montserrat_32;
    const lv_font_t *aux=s_notice_aux_font?s_notice_aux_font:&lv_font_montserrat_32;
    lv_obj_t *label=lv_label_create(s_notice_content);
    lv_label_set_text(label,s_language?"Unexpected restart":"设备异常重启");
    label_style(label,title,0xf2c76b,TP_SYSTEM_TITLE_Y);
    lv_obj_t *hint=lv_label_create(s_notice_content);
    lv_label_set_text(hint,s_language?"Diagnostic record saved. View it in device settings.":"已保留故障记录，可在网页设备设置中查看和导出。");
    label_style(hint,aux,0x8aa4b8,TP_SYSTEM_DETAIL_Y);
    startup_screen_tick();
}

void startup_screen_dismiss_notice(void)
{
    if (s_notice) lv_obj_delete(s_notice);
    s_notice = NULL;
    s_notice_content=NULL;
    if (s_notice_font) lv_tiny_ttf_destroy(s_notice_font);
    if (s_notice_aux_font) lv_tiny_ttf_destroy(s_notice_aux_font);
    s_notice_font = NULL;
    s_notice_aux_font=NULL;
    s_update_bar=s_update_label=s_update_percent=NULL;
    s_update_stage=s_update_value=0;
}

void startup_screen_firmware(unsigned stage, unsigned percent)
{
    if(!s_update_bar){
        startup_screen_notice("正在更新", "");
        s_update_label=lv_label_create(s_notice_content);
        label_style(s_update_label,s_notice_aux_font?s_notice_aux_font:&lv_font_montserrat_32,0x8aa4b8,TP_SYSTEM_DETAIL_Y);
        s_update_bar=lv_bar_create(s_notice_content);
        lv_obj_set_size(s_update_bar,lv_pct(78),8);
        lv_obj_align(s_update_bar,LV_ALIGN_CENTER,0,TP_SYSTEM_PROGRESS_Y);
        lv_obj_set_style_bg_color(s_update_bar,lv_color_hex(0x263442),LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_update_bar,lv_color_hex(0xcbef9c),LV_PART_INDICATOR);
        s_update_percent=lv_label_create(s_notice_content);
        label_style(s_update_percent,s_notice_aux_font?s_notice_aux_font:&lv_font_montserrat_32,0xcbef9c,TP_SYSTEM_PERCENT_Y);
        s_update_stage=99;
    }
    if(stage==s_update_stage&&percent==s_update_value)return;
    bool changed=stage!=s_update_stage;
    s_update_stage=stage;s_update_value=percent;
    lv_label_set_text(s_update_label,stage==TP_UPDATE_CHECKING?(s_language?"Checking SD card":"正在检查 SD 卡"):
        stage==TP_UPDATE_RESTARTING?(s_language?"Verified / Restarting":"校验完成，即将重启"):
        (s_language?"Writing firmware":"正在写入固件"));
    lv_obj_align(s_update_label,LV_ALIGN_CENTER,0,TP_SYSTEM_DETAIL_Y);
    ui_motion_progress(s_update_bar,percent);
    lv_label_set_text_fmt(s_update_percent,"%u%%",percent);
    lv_obj_align(s_update_percent,LV_ALIGN_CENTER,0,TP_SYSTEM_PERCENT_Y);
    if(changed)ui_motion_enter(s_update_label,0);
}

void startup_screen_power(unsigned percent,const char *status,const char *detail)
{
    if(!s_update_bar){
        startup_screen_notice("正在开机","");
        s_update_label=lv_label_create(s_notice_content);
        label_style(s_update_label,s_notice_aux_font?s_notice_aux_font:&lv_font_montserrat_32,0x8aa4b8,TP_SYSTEM_DETAIL_Y);
        s_update_bar=lv_bar_create(s_notice_content);lv_obj_set_size(s_update_bar,lv_pct(78),8);
        lv_obj_align(s_update_bar,LV_ALIGN_CENTER,0,TP_SYSTEM_PROGRESS_Y);
        lv_obj_set_style_bg_color(s_update_bar,lv_color_hex(0x263442),LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_update_bar,lv_color_hex(0xcbef9c),LV_PART_INDICATOR);
        s_update_percent=lv_label_create(s_notice_content);
        label_style(s_update_percent,s_notice_aux_font?s_notice_aux_font:&lv_font_montserrat_32,0xcbef9c,TP_SYSTEM_PERCENT_Y);
    }
    lv_label_set_text(s_update_label,s_language?detail:status);
    lv_obj_align(s_update_label,LV_ALIGN_CENTER,0,TP_SYSTEM_DETAIL_Y);
    ui_motion_progress(s_update_bar,percent);lv_label_set_text_fmt(s_update_percent,"%u%%",percent);
    lv_obj_align(s_update_percent,LV_ALIGN_CENTER,0,TP_SYSTEM_PERCENT_Y);
    startup_screen_tick();
}
