#include "ui_motion.h"

static void reveal(void *object, int32_t value)
{
    lv_obj_set_style_opa(object,(lv_opa_t)value,0);
    lv_obj_set_style_translate_y(object,(255-value)*TP_MOTION_LIFT_PX/255,0);
}

static void progress(void *bar, int32_t value)
{
    lv_bar_set_value(bar,value,LV_ANIM_OFF);
}

static void slide_left(void *object, int32_t value)
{
    lv_obj_set_style_opa(object,(lv_opa_t)value,0);
    lv_obj_set_style_translate_x(object,(255-value)*TP_MOTION_GUIDE_SLIDE_PX/255,0);
}

void ui_motion_slide_left(lv_obj_t *object)
{
    lv_anim_delete(object,reveal);
    lv_anim_delete(object,slide_left);
    lv_obj_set_style_translate_y(object,0,0);
    lv_anim_t anim;lv_anim_init(&anim);
    lv_anim_set_var(&anim,object);lv_anim_set_exec_cb(&anim,slide_left);
    lv_anim_set_values(&anim,0,255);lv_anim_set_duration(&anim,TP_MOTION_ENTER_MS);
    lv_anim_set_path_cb(&anim,lv_anim_path_ease_out);
    if(!lv_anim_start(&anim))slide_left(object,255);
}

static void pulse(void *object, int32_t value)
{
    lv_obj_set_style_opa(object,(lv_opa_t)value,0);
}

static void rotate_arc(void *object, int32_t value)
{
    lv_arc_set_rotation(object,value);
}

static void reveal_start(lv_obj_t *object, bool showing, unsigned delay_ms)
{
    lv_anim_delete(object,reveal);
    lv_anim_t anim;lv_anim_init(&anim);
    lv_anim_set_var(&anim,object);lv_anim_set_exec_cb(&anim,reveal);
    lv_anim_set_values(&anim,showing?0:255,showing?255:0);
    lv_anim_set_duration(&anim,showing?TP_MOTION_ENTER_MS:TP_MOTION_EXIT_MS);
    lv_anim_set_delay(&anim,delay_ms);
    lv_anim_set_path_cb(&anim,showing?lv_anim_path_ease_out:lv_anim_path_ease_in);
    /* An allocation failure must never leave useful UI invisible. */
    if(!lv_anim_start(&anim))reveal(object,showing?255:0);
}

void ui_motion_enter(lv_obj_t *object, unsigned delay_ms)
{
    reveal_start(object,true,delay_ms);
}

void ui_motion_exit(lv_obj_t *object)
{
    reveal_start(object,false,0);
}

void ui_motion_stop(lv_obj_t *object)
{
    lv_anim_delete(object,reveal);
    lv_anim_delete(object,slide_left);
    lv_anim_delete(object,pulse);
    lv_anim_delete(object,rotate_arc);
    reveal(object,255);
    lv_obj_set_style_translate_x(object,0,0);
}

void ui_motion_wait(lv_obj_t *object, bool waiting)
{
    lv_anim_delete(object,pulse);
    if(!waiting){pulse(object,255);return;}
    lv_anim_t anim;lv_anim_init(&anim);
    lv_anim_set_var(&anim,object);lv_anim_set_exec_cb(&anim,pulse);
    lv_anim_set_values(&anim,85,255);lv_anim_set_duration(&anim,TP_MOTION_PULSE_MS);
    lv_anim_set_reverse_duration(&anim,TP_MOTION_PULSE_MS);
    lv_anim_set_repeat_count(&anim,LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&anim,lv_anim_path_ease_in_out);
    if(!lv_anim_start(&anim))pulse(object,255);
}

void ui_motion_progress(lv_obj_t *bar, unsigned percent)
{
    if(percent>100)percent=100;
    int32_t start=lv_bar_get_value(bar);
    lv_anim_delete(bar,progress);
    if(start==(int32_t)percent)return;
    lv_anim_t anim;lv_anim_init(&anim);
    lv_anim_set_var(&anim,bar);lv_anim_set_exec_cb(&anim,progress);
    lv_anim_set_values(&anim,start,(int32_t)percent);
    lv_anim_set_duration(&anim,TP_MOTION_PROGRESS_MS);
    lv_anim_set_path_cb(&anim,lv_anim_path_ease_out);
    if(!lv_anim_start(&anim))progress(bar,(int32_t)percent);
}

void ui_motion_spin(lv_obj_t *arc)
{
    lv_anim_delete(arc,rotate_arc);
    lv_anim_t anim;lv_anim_init(&anim);
    lv_anim_set_var(&anim,arc);lv_anim_set_exec_cb(&anim,rotate_arc);
    lv_anim_set_values(&anim,270,630);
    lv_anim_set_duration(&anim,TP_MOTION_SPIN_MS);
    lv_anim_set_path_cb(&anim,lv_anim_path_linear);
    lv_anim_set_repeat_count(&anim,LV_ANIM_REPEAT_INFINITE);
    if(!lv_anim_start(&anim))rotate_arc(arc,270);
}
