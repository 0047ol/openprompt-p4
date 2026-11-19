#pragma once
#include "esp_err.h"
#include "esp_lcd_mipi_dsi.h"

esp_err_t board_display_init(esp_lcd_panel_handle_t *panel);
