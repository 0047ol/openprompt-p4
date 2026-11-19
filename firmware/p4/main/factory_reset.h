#pragma once
#include <stdbool.h>
#include "esp_err.h"

/* Apply before any settings are loaded. Never reads or writes the SD card. */
esp_err_t factory_reset_apply_pending(void);
/* Caller holds the firmware update lock and has excluded OTA/SD maintenance. */
esp_err_t factory_reset_prepare(void);
bool factory_reset_pending(void);
