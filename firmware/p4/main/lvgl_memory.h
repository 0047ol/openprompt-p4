#pragma once
#include "esp_err.h"

/* Validate PSRAM and pull the custom allocator into the static link. */
esp_err_t lvgl_memory_init(void);
