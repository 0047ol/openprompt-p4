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
void display_port_set_standby(bool standby);

#ifdef __cplusplus
}
#endif
