#include <inttypes.h>
#include "board_display.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/mipi_dsi_host_ll.h"
#include "jd9365_board_sequence.h"

static const char *TAG = "board_display";
static esp_lcd_dsi_bus_handle_t s_bus;
static esp_ldo_channel_handle_t s_ldo;

/* IDF 6.1's public DBI read uses unbounded FIFO waits. This diagnostic uses
 * the same ESP32-P4 packet interface with explicit deadlines and yields.
 * USB/heartbeat tasks remain live when the LCD does not answer. */
static esp_err_t wait_tx_ready(void)
{
    int64_t deadline = esp_timer_get_time() + 250000;
    while (mipi_dsi_host_ll_gen_is_cmd_fifo_full(&MIPI_DSI_HOST)) {
        if (esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }
    return ESP_OK;
}

static esp_err_t short_packet(mipi_dsi_data_type_t type, uint8_t data0, uint8_t data1)
{
    ESP_RETURN_ON_ERROR(wait_tx_ready(), TAG, "TX FIFO timeout");
    mipi_dsi_host_ll_gen_set_packet_header(&MIPI_DSI_HOST, 0, type, data1, data0);
    return ESP_OK;
}

static esp_err_t write_register(uint8_t command, uint8_t value)
{
    return short_packet(MIPI_DSI_DT_DCS_SHORT_WRITE_1, command, value);
}

static esp_err_t read_id(uint8_t id[3])
{
    mipi_dsi_host_ll_enable_video_mode(&MIPI_DSI_HOST, false);
    mipi_dsi_host_ll_enable_bta(&MIPI_DSI_HOST, true);
    mipi_dsi_host_ll_gen_set_rx_vcid(&MIPI_DSI_HOST, 0);
    ESP_RETURN_ON_ERROR(short_packet(MIPI_DSI_DT_SET_MAXIMUM_RETURN_PKT, 3, 0),
                        TAG, "set return packet size");
    ESP_RETURN_ON_ERROR(short_packet(MIPI_DSI_DT_DCS_READ_0, 0x04, 0),
                        TAG, "request LCD ID");
    int64_t deadline = esp_timer_get_time() + 500000;
    while (mipi_dsi_host_ll_gen_is_read_cmd_busy(&MIPI_DSI_HOST) ||
           mipi_dsi_host_ll_gen_is_read_fifo_empty(&MIPI_DSI_HOST)) {
        if (esp_timer_get_time() >= deadline) {
            ESP_LOGW(TAG, "LCD_ID_TIMEOUT phy=0x%08" PRIx32
                     " fifo=0x%08" PRIx32 " irq0=0x%08" PRIx32
                     " irq1=0x%08" PRIx32,
                     MIPI_DSI_HOST.phy_status.val, MIPI_DSI_HOST.cmd_pkt_status.val,
                     MIPI_DSI_HOST.int_st0.val, MIPI_DSI_HOST.int_st1.val);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }
    uint32_t value = mipi_dsi_host_ll_gen_read_payload_fifo(&MIPI_DSI_HOST);
    for (int i = 0; i < 3; ++i) {
        id[i] = (value >> (8 * i)) & 0xff;
    }
    mipi_dsi_host_ll_enable_bta(&MIPI_DSI_HOST, false);
    ESP_LOGI(TAG, "LCD_ID %02X %02X %02X", id[0], id[1], id[2]);
    return ESP_OK;
}

static esp_err_t create_bus(void)
{
    const esp_lcd_dsi_bus_config_t config = {
        .bus_id = 0,
        .num_data_lanes = 2,
        .lane_bit_rate_mbps = 960,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&config, &s_bus), TAG, "DSI bus");
    const esp_lcd_dbi_io_config_t io_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(s_bus, &io_config, &io), TAG, "DBI IO");
    /* The short-packet functions below use the DBI setup without its
       unbounded reads/writes. A missing response must not block this test. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_del(io), TAG, "release DBI wrapper");
    mipi_dsi_host_ll_enable_cmd_ack(&MIPI_DSI_HOST, false);
    mipi_dsi_host_ll_enable_bta(&MIPI_DSI_HOST, false);
    return ESP_OK;
}

static esp_err_t create_dpi_panel(esp_lcd_panel_handle_t *panel)
{
    /* Keep the verified exact pixel-clock divider. */
    const esp_lcd_dpi_panel_config_t config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 60,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 2,
        .video_timing = {
            .h_size = 800, .v_size = 1280,
            .hsync_back_porch = 20, .hsync_pulse_width = 20, .hsync_front_porch = 40,
            .vsync_back_porch = 10, .vsync_pulse_width = 4, .vsync_front_porch = 30,
        },
    };
    ESP_LOGI(TAG, "DISPLAY_STAGE allocate DPI framebuffer while panel is in reset");
    return esp_lcd_new_panel_dpi(s_bus, &config, panel);
}

esp_err_t board_display_init(esp_lcd_panel_handle_t *panel)
{
    const gpio_config_t reset = {
        .pin_bit_mask = 1ULL << GPIO_NUM_5,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&reset), TAG, "reset GPIO");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_5, 0), TAG, "reset low");
    vTaskDelay(pdMS_TO_TICKS(20));
    const esp_ldo_channel_config_t ldo = {.chan_id = 3, .voltage_mv = 2500};
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo, &s_ldo), TAG, "DPHY LDO");
    ESP_RETURN_ON_ERROR(create_bus(), TAG, "DSI bus");
    /* Match the supplier's usb_extend_screen sequence: allocate/clear the
     * PSRAM framebuffer before waking the LCD and enabling its scan driver. */
    ESP_RETURN_ON_ERROR(create_dpi_panel(panel), TAG, "DPI panel");
    ESP_LOGI(TAG, "DISPLAY_STAGE framebuffer ready; releasing panel reset");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_5, 1), TAG, "reset high");
    vTaskDelay(pdMS_TO_TICKS(120));
    uint8_t id[3];
    if (read_id(id) != ESP_OK) {
        ESP_LOGW(TAG, "LCD ID unavailable; continuing with verified JD9365 sequence");
        mipi_dsi_host_ll_enable_bta(&MIPI_DSI_HOST, false);
    }
    for (size_t i = 0; i < sizeof(panel_commands) / sizeof(panel_commands[0]); ++i) {
        if (panel_commands[i].cmd == 0x11 && panel_commands[i].delay_ms == 120) {
            ESP_LOGI(TAG, "DISPLAY_STAGE panel sleep-out command");
        }
        if (panel_commands[i].cmd == 0x29 && panel_commands[i].delay_ms == 20) {
            ESP_LOGI(TAG, "DISPLAY_STAGE panel display-on command");
        }
        ESP_RETURN_ON_ERROR(write_register(panel_commands[i].cmd, panel_commands[i].value),
                            TAG, "panel command %u", (unsigned)i);
        vTaskDelay(pdMS_TO_TICKS(panel_commands[i].delay_ms) + 1);
    }
    ESP_RETURN_ON_ERROR(write_register(0xE0, 1), TAG, "page 1");
    ESP_RETURN_ON_ERROR(write_register(0x4A, 0), TAG, "BIST off");
    ESP_RETURN_ON_ERROR(write_register(0xE0, 0), TAG, "user page");
    ESP_RETURN_ON_ERROR(write_register(0x3A, 0x55), TAG, "RGB565");

    mipi_dsi_host_ll_dpi_enable_frame_ack(&MIPI_DSI_HOST, false);
    ESP_LOGI(TAG, "DISPLAY_STAGE start DPI scanout");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(*panel), TAG, "start DPI");
    ESP_LOGI(TAG, "800x1280 RGB565 DPI=60MHz lanes=2x960Mbps ready");
    return ESP_OK;
}

