#pragma once

#include <stdbool.h>
#include "esp_err.h"

/* Called by app_main and the serialized HTTP task. One attempt per boot. */
esp_err_t teleprompter_start_display(void);
bool teleprompter_display_can_retry(void);
typedef enum {
    TP_DEVICE_NONE, TP_DEVICE_RESTART, TP_DEVICE_STANDBY, TP_DEVICE_WAKE, TP_DEVICE_DOWNLOAD, TP_DEVICE_SHUTDOWN
} tp_device_action_t;
esp_err_t teleprompter_device_action(tp_device_action_t action);
const char *teleprompter_lifecycle(void);
