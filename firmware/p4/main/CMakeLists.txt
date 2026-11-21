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
    SRCS "main.c" "display_port.c" "display_capture.c" "${tp_board_display}" "teleprompter_ui.c"
         "prompter_state.c" "network_control.c" "wifi_settings.c" "lvgl_memory.c" "bluetooth_control.c"
         "tp_dma2d_prepared.c" "startup_screen.c" "ui_motion.c" "device_settings.c" "script_library.c" "firmware_update.c" "factory_reset.c" "captive_portal.c" "web_assets.c" "control_session.c" "connection_guide.c" "crash_log.c"
         "../../../shared/protocol/teleprompter_protocol.c"
    INCLUDE_DIRS "." "../../../shared/protocol"
    REQUIRES ${tp_requires}
    EMBED_TXTFILES "web_editor.html"
    EMBED_FILES "noto-sc-subset.otf" "web_editor.html.gz" "reader_shards.bin"
)

# Keep debug symbols and assertions, but optimize the drawing/rotation hot paths.
target_compile_options(${COMPONENT_LIB} PRIVATE -O2)
# Intercept the public-to-port panic call without modifying the installed SDK.
# The project wrapper immediately chains to IDF's original panic handler.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")
if(TP_SD_BOUNDARY_TEST)
    # The wrapper includes the production library and adds USB test controls.
    # The normal binary contains neither those controls nor the test worker.
    get_target_property(tp_main_sources ${COMPONENT_LIB} SOURCES)
    list(FILTER tp_main_sources EXCLUDE REGEX "[/\\]script_library\\.c$")
    list(FILTER tp_main_sources EXCLUDE REGEX "^script_library\\.c$")
    set_property(TARGET ${COMPONENT_LIB} PROPERTY SOURCES ${tp_main_sources})
    target_sources(${COMPONENT_LIB} PRIVATE "../../../tools/library-host/sd-boundary-device.c")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE TP_SD_BOUNDARY_TEST=1)
endif()
if(TP_SD_READONLY_BACKUP)
    target_sources(${COMPONENT_LIB} PRIVATE "sd_readonly_backup.c")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE TP_SD_READONLY_BACKUP=1)
endif()
