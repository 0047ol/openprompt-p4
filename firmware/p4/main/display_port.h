#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t display_port_init(void);
void display_port_tick(void);
lv_display_t *display_port_get_lvgl_display(void);
uint32_t display_port_get_refresh_count(void);
bool display_port_begin_frame(bool viewport_changed);
/* Whether chrome copied to this back buffer overlaps a stationary overlay. */
bool display_port_frame_region_changed(int x,int y,int width,int height);
void display_port_blit(int x, int y, int width, int height,
                       const uint16_t *source, int stride, int source_height, int sx, int sy);
void display_port_present(void);
/* On-demand copy of a completed panel frame. Caller frees pixels with free().
 * Native RGB565 is 800x1280; rotation describes the logical UI orientation. */
typedef struct {
    uint16_t *pixels;
    unsigned width, height, rotation;
    uint32_t frame;
    int64_t captured_us;
    bool asleep;
} tp_display_capture_t;
esp_err_t display_port_capture(tp_display_capture_t *capture, uint32_t timeout_ms);
esp_err_t display_port_capture_scaled(tp_display_capture_t *capture, unsigned scale, uint32_t timeout_ms);
/* UI task only; also serviced while asleep, without waking the panel. */
void display_port_capture_service(void);
void display_port_set_standby(bool standby);
/* Early boot and shutdown keep the backlight dark across peripheral reset. */
void display_port_boot_dark(void);
void display_port_power_off(void);
/* Translate an already-rendered landscape guide using DMA, without redrawing
 * its QR and glyphs at every animation step. Allocation failure is recoverable. */
bool display_port_guide_slide_begin(const lv_area_t *content,lv_color_t background,int distance);
void display_port_guide_slide_draw(int offset);
void display_port_guide_slide_end(void);

#ifdef __cplusplus
}
#endif
