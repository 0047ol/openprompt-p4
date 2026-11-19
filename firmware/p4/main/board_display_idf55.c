/* IDF 5.5 comparison path using the supplier's unmodified JD9365 driver. */
#include "board_display.h"
#include "esp_check.h"
#include "esp_lcd_jd9365.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"

static const char *TAG = "supplier_display";
static esp_ldo_channel_handle_t s_ldo;
static esp_lcd_dsi_bus_handle_t s_bus;
static esp_lcd_panel_io_handle_t s_io;

esp_err_t board_display_init(esp_lcd_panel_handle_t *panel)
{
    const esp_ldo_channel_config_t ldo = {.chan_id = 3, .voltage_mv = 2500};
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo, &s_ldo), TAG, "DPHY LDO");
    const esp_lcd_dsi_bus_config_t bus = JD9365_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus, &s_bus), TAG, "DSI bus");
    const esp_lcd_dbi_io_config_t dbi = JD9365_PANEL_IO_DBI_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(s_bus, &dbi, &s_io), TAG, "DBI IO");

    esp_lcd_dpi_panel_config_t dpi = JD9365_800_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    /* This panel has already displayed correctly with an exact 60 MHz divider. */
    dpi.dpi_clock_freq_mhz = 60;
    dpi.num_fbs = 2;
    dpi.flags.use_dma2d = false;
    jd9365_vendor_config_t vendor = {
        .mipi_config = {.dsi_bus = s_bus, .dpi_config = &dpi, .lane_num = 2},
    };
    const esp_lcd_panel_dev_config_t config = {
        .reset_gpio_num = 5,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    ESP_LOGI(TAG, "SUPPLIER_STAGE create JD9365 panel/framebuffer");
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_jd9365(s_io, &config, panel), TAG, "JD9365 create");
    ESP_LOGI(TAG, "SUPPLIER_STAGE reset GPIO5");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(*panel), TAG, "JD9365 reset");
    ESP_LOGI(TAG, "SUPPLIER_STAGE initialize supplier command table and DPI");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*panel), TAG, "JD9365 init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(*panel, true, true), TAG, "JD9365 orientation");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(*panel, true), TAG, "JD9365 on");
    ESP_LOGI(TAG, "SUPPLIER_STAGE display initialized 800x1280 60MHz");
    return ESP_OK;
}
