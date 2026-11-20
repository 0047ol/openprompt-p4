#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t teleprompter_ui_init(void);
void teleprompter_ui_set_text(const char *text);
void teleprompter_ui_tick(void);
void teleprompter_ui_show_guide(bool visible);
/* Call when the startup/update/sleep overlay has actually been removed. */
void teleprompter_ui_resume_motions(void);
bool teleprompter_ui_present(void);
esp_err_t teleprompter_preview_init(void);
/* JSON snapshot allocated for the caller; LVGL is accessed by its owner only. */
char *teleprompter_preview_snapshot(void);
char *teleprompter_telemetry_snapshot(void);

#ifdef __cplusplus
}
#endif
