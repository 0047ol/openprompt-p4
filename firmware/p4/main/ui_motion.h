#pragma once
#include "lvgl.h"
#include "ui_motion_settings.h"

/* Small transient effects only; never animate the script surface or QR pixels. */
void ui_motion_enter(lv_obj_t *object, unsigned delay_ms);
void ui_motion_slide_left(lv_obj_t *object);
void ui_motion_exit(lv_obj_t *object);
void ui_motion_stop(lv_obj_t *object);
void ui_motion_wait(lv_obj_t *object, bool waiting);
void ui_motion_spin(lv_obj_t *arc);
void ui_motion_progress(lv_obj_t *bar, unsigned percent);
