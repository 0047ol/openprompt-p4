set(tp_requires driver esp_driver_spi sdmmc fatfs esp_driver_ledc esp_driver_ppa esp_hw_support esp_lcd esp_psram esp_mm espressif__mdns
    app_update esp_app_format mbedtls esp_timer esp_tinyusb freertos log esp_wifi esp_netif esp_event esp_http_server nvs_flash spiffs bt esp_hosted espressif__cjson)
set(tp_board_display "board_display.c")
if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND tp_requires esp_hal_lcd esp_driver_dma)
else()
    list(APPEND tp_requires esp_wifi_remote esp_lcd_jd9365)
    set(tp_board_display "board_display_idf55.c")
endif()

idf_component_register(
    SRCS "main.c" "display_port.c" "${tp_board_display}" "teleprompter_ui.c"
         "prompter_state.c" "network_control.c" "wifi_settings.c" "lvgl_memory.c" "bluetooth_control.c"
         "tp_dma2d_prepared.c" "startup_screen.c" "ui_motion.c" "device_settings.c" "script_library.c" "firmware_update.c" "factory_reset.c" "captive_portal.c" "web_assets.c" "control_session.c" "connection_guide.c"
         "../../../shared/protocol/teleprompter_protocol.c"
    INCLUDE_DIRS "." "../../../shared/protocol"
    REQUIRES ${tp_requires}
    EMBED_TXTFILES "web_editor.html"
    EMBED_FILES "noto-sc-subset.otf" "web_editor.html.gz" "reader_shards.bin"
)

# Keep debug symbols and assertions, but optimize the drawing/rotation hot paths.
target_compile_options(${COMPONENT_LIB} PRIVATE -O2)
