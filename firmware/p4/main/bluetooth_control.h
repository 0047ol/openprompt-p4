#pragma once
#include <stdbool.h>
#include "esp_err.h"

/* Start after ESP-Hosted has connected to the existing C6 firmware. */
esp_err_t bluetooth_control_start(void);
/* Temporary USB diagnostic; never changes saved settings. */
esp_err_t bluetooth_control_diagnostic_pause(bool paused);
esp_err_t bluetooth_control_diagnostic_advertising(bool slow);
esp_err_t bluetooth_control_refresh_name(const char *name);
