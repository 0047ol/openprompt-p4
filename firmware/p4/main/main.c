#include <inttypes.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "nimble/nimble_port.h"
#include "esp_private/esp_psram_extram.h"
#include "lvgl.h"
#include "soc/lp_system_reg.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_console.h"
#include "tinyusb_default_config.h"

#include "display_port.h"
#include "teleprompter_ui.h"
#include "network_control.h"
#include "wifi_settings.h"
#include "bluetooth_control.h"
#include "prompter_state.h"
#include "lvgl_memory.h"
#include "app_control.h"
#include "factory_reset.h"
#include "nvs_flash.h"
#include "startup_screen.h"
#include "ui_motion_settings.h"
#include "device_settings.h"
#include "script_library.h"
#include "firmware_update.h"

static const char *TAG = "teleprompter_p4";
static void detach_usb_on_restart(void)
{
    display_port_power_off();
    /* Give the host time to remove CDC before the USB controller resets. */
    tud_disconnect();
    vTaskDelay(pdMS_TO_TICKS(350));
}
static TaskHandle_t s_download_task;
static bool s_display_attempted;
static TaskHandle_t s_network_task;
enum { NET_WAITING, NET_STARTING, NET_WIFI_READY, NET_READY, NET_WIFI_FAILED, NET_BLE_FAILED };
static atomic_int s_network_phase;
static atomic_int s_device_action;
static atomic_bool s_ui_running, s_download_notice_done;
static atomic_bool s_screen_asleep;

/* Local USB diagnostics stay available when the radio path is unhealthy. */
static void usb_network_diagnostic(const char *command)
{
    if(!strcmp(command,"netscan")){
        if(atomic_load(&s_network_phase)<NET_WIFI_READY||firmware_update_in_progress())return;
        wifi_config_t station={0};
        if(esp_wifi_get_config(WIFI_IF_STA,&station)!=ESP_OK)return;
        uint8_t scan_ssid[33]={0};memcpy(scan_ssid,station.sta.ssid,32);
        memset(&station,0,sizeof(station));
        if(!scan_ssid[0])return;
        wifi_scan_config_t scan={.ssid=scan_ssid,.scan_type=WIFI_SCAN_TYPE_ACTIVE};
        esp_err_t result=esp_wifi_scan_start(&scan,true);
        ESP_LOGI(TAG,"NETSCAN result=%s",esp_err_to_name(result));
        if(result==ESP_OK){
            wifi_ap_record_t *records=calloc(16,sizeof(*records));uint16_t count=16;
            if(!records)return;
            result=esp_wifi_scan_get_ap_records(&count,records);
            if(result==ESP_OK)for(unsigned i=0;i<count;++i){
                const wifi_ap_record_t *ap=&records[i];
                ESP_LOGI(TAG,"NETSCAN rssi=%d channel=%u auth=%d bssid=%02x:%02x:%02x:%02x:%02x:%02x",
                    ap->rssi,ap->primary,ap->authmode,ap->bssid[0],ap->bssid[1],ap->bssid[2],ap->bssid[3],ap->bssid[4],ap->bssid[5]);
            }
            free(records);
        }
        memset(&station,0,sizeof(station));return;
    }
    if (!strcmp(command,"linkap")||!strcmp(command,"linklan")||!strcmp(command,"linkble")) {
        if(atomic_load(&s_network_phase)>=NET_WIFI_READY)
            ESP_LOGI(TAG,"USB connection mode: %s",esp_err_to_name(wifi_settings_switch_mode(!strcmp(command,"linkap")?"hotspot":!strcmp(command,"linklan")?"lan":"bluetooth")));
        return;
    }
    if (!strcmp(command, "advslow") || !strcmp(command, "advnormal")) {
        ESP_LOGI(TAG, "USB diagnostic advertising request: %s", esp_err_to_name(
            bluetooth_control_diagnostic_advertising(!strcmp(command, "advslow"))));
        return;
    }
    if (!strcmp(command, "bleoff") || !strcmp(command, "bleon")) {
        ESP_LOGI(TAG, "USB diagnostic Bluetooth request: %s", esp_err_to_name(
            bluetooth_control_diagnostic_pause(!strcmp(command, "bleoff"))));
        return;
    }
    if (strcmp(command, "netdiag") && strcmp(command, "netawake") && strcmp(command,"netbalanced")) return;
    if (atomic_load(&s_network_phase) < NET_WIFI_READY) return;
    if (!strcmp(command, "netawake"))
        ESP_LOGI(TAG, "NETDIAG disable power save: %s", esp_err_to_name(esp_wifi_set_ps(WIFI_PS_NONE)));
    if (!strcmp(command, "netbalanced"))
        ESP_LOGI(TAG, "NETDIAG default modem policy: %s", esp_err_to_name(esp_wifi_set_ps(WIFI_PS_MIN_MODEM)));
    wifi_ps_type_t power = WIFI_PS_MIN_MODEM;
    wifi_mode_t mode = WIFI_MODE_NULL;
    wifi_ap_record_t ap = {0};
    esp_err_t power_err = esp_wifi_get_ps(&power);
    esp_err_t mode_err = esp_wifi_get_mode(&mode);
    esp_err_t ap_err = esp_wifi_sta_get_ap_info(&ap);
    wifi_config_t station={0};
    if(esp_wifi_get_config(WIFI_IF_STA,&station)==ESP_OK)
        ESP_LOGI(TAG,"NETDIAG scan=%d sort=%d threshold=%d retries=%u pinned=%d",
            station.sta.scan_method,station.sta.sort_method,station.sta.threshold.rssi,
            station.sta.failure_retry_cnt,station.sta.bssid_set);
    memset(&station,0,sizeof(station));
    ESP_LOGI(TAG, "NETDIAG power=%d/%s mode=%d/%s rssi=%d channel=%u ap=%s "
             "bssid=%02x:%02x:%02x:%02x:%02x:%02x heap=%lu",
             (int)power, esp_err_to_name(power_err), (int)mode, esp_err_to_name(mode_err),
             (int)ap.rssi, (unsigned)ap.primary, esp_err_to_name(ap_err),
             ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5],
             (unsigned long)esp_get_free_heap_size());
}

static void usb_console_task(void *arg)
{
    (void)arg;
    char command[32];
    size_t used = 0;
    for (;;) {
        uint8_t bytes[32]; size_t received = 0;
        if (tinyusb_cdcacm_read(TINYUSB_CDC_ACM_0, bytes, sizeof(bytes), &received) == ESP_OK) {
            for (size_t i = 0; i < received; ++i) {
                if (bytes[i] == '\n' || bytes[i] == '\r') {
                    command[used] = 0;
                    if (used) usb_network_diagnostic(command);
                    used = 0;
                } else if (used + 1 < sizeof(command)) command[used++] = bytes[i];
                else used = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

const char *teleprompter_lifecycle(void)
{
    if(factory_reset_pending())return "resetting";
    int action=atomic_load(&s_device_action);
    if(action==TP_DEVICE_RESTART)return "restarting";
    if(action==TP_DEVICE_SHUTDOWN)return "shutting_down";
    if(firmware_update_in_progress())return "updating";
    if(action==TP_DEVICE_DOWNLOAD)return "updating";
    if(action==TP_DEVICE_STANDBY)return "sleeping";
    if(action==TP_DEVICE_WAKE)return "waking";
    if(atomic_load(&s_screen_asleep))return "asleep";
    return atomic_load(&s_ui_running)?"ready":"starting";
}

esp_err_t teleprompter_device_action(tp_device_action_t action)
{
    if(factory_reset_pending())return ESP_ERR_INVALID_STATE;
    if (action <= TP_DEVICE_NONE || action > TP_DEVICE_SHUTDOWN) return ESP_ERR_INVALID_ARG;
    if (!atomic_load(&s_ui_running)) return ESP_ERR_INVALID_STATE;
    if(action!=TP_DEVICE_RESTART&&firmware_update_in_progress())return ESP_ERR_INVALID_STATE;
    if(action==TP_DEVICE_SHUTDOWN&&!firmware_update_allows_shutdown())return ESP_ERR_INVALID_STATE;
    if(action==TP_DEVICE_RESTART&&!firmware_update_allows_restart())return ESP_ERR_INVALID_STATE;
    device_settings_activity();
    int expected = TP_DEVICE_NONE;
    return atomic_compare_exchange_strong(&s_device_action, &expected, action) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static void network_task(void *arg)
{
    (void)arg;
    /* Begin once a visible startup page exists. Keep a bounded fallback for
     * power-recovery boots where the display is intentionally not started. */
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(6000));
    atomic_store(&s_network_phase, NET_STARTING);
    esp_err_t err = network_control_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Network startup stopped: %s; display/USB remain available", esp_err_to_name(err));
        atomic_store(&s_network_phase, NET_WIFI_FAILED);
    } else {
        atomic_store(&s_network_phase, NET_WIFI_READY);
        err = bluetooth_control_start();
        if (err != ESP_OK) ESP_LOGW(TAG, "Bluetooth startup: %s", esp_err_to_name(err));
        atomic_store(&s_network_phase, err == ESP_OK ? NET_READY : NET_BLE_FAILED);
    }
    vTaskDelete(NULL);
}

static uint32_t lvgl_tick_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void usb_download_task(void *arg)
{
    (void)arg;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        atomic_store(&s_download_notice_done, false);
        if (teleprompter_device_action(TP_DEVICE_DOWNLOAD) == ESP_OK) {
            int64_t deadline = esp_timer_get_time() + 2500000;
            while (!atomic_load(&s_download_notice_done) && esp_timer_get_time() < deadline)
                vTaskDelay(pdMS_TO_TICKS(20));
        }
        vTaskDelay(pdMS_TO_TICKS(250));
        tud_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        REG_SET_BIT(LP_SYSTEM_REG_SYS_CTRL_REG, LP_SYSTEM_REG_FORCE_DOWNLOAD_BOOT);
        esp_rom_software_reset_system();
    }
}

static void usb_line_coding_changed(int interface, cdcacm_event_t *event)
{
    (void)interface;
    if (event->type == CDC_EVENT_LINE_CODING_CHANGED &&
        event->line_coding_changed_data.p_line_coding->bit_rate == 1200) {
        ESP_LOGI(TAG, "USB CDC 1200-baud request: rebooting to ROM download mode");
        xTaskNotifyGive(s_download_task);
    }
}

static void display_task(void *arg)
{
    (void)arg;
    /* Keep download/USB alive before probing external memory or the panel. */
    vTaskDelay(pdMS_TO_TICKS(500));
#if !CONFIG_SPIRAM_BOOT_INIT
    ESP_ERROR_CHECK(esp_psram_init());
    ESP_ERROR_CHECK(esp_psram_extram_add_to_heap_allocator());
#endif
    ESP_LOGI(TAG, "PSRAM ready: %u bytes", (unsigned)esp_psram_get_size());

    ESP_ERROR_CHECK(lvgl_memory_init());
    lv_init();
    lv_tick_set_cb(lvgl_tick_ms);

    ESP_ERROR_CHECK(display_port_init());
    ESP_ERROR_CHECK(startup_screen_init());
    ESP_LOGI(TAG, "STARTUP_SCREEN visible");
    script_library_startup_begin();
    if (atomic_load(&s_network_phase) == NET_WAITING && s_network_task)
        xTaskNotifyGive(s_network_task);
    int phase = -1;
    unsigned storage_phase = UINT_MAX;
    int64_t startup_us = esp_timer_get_time();
    int64_t finished_us = 0;
    while (esp_timer_get_time() - startup_us < 6500000) {
        int next = atomic_load(&s_network_phase);
        unsigned storage_next=script_library_startup_stage();
        if (next != phase || storage_next != storage_phase) {
            phase = next;
            storage_phase=storage_next;
            ESP_LOGI(TAG,"STARTUP_STEP network=%d storage=%u elapsed_ms=%" PRId64,
                     phase,storage_phase,(esp_timer_get_time()-startup_us)/1000);
            if(storage_phase==1)
                startup_screen_update(55,"正在准备稿件存储","Preparing manuscript storage");
            else if(storage_phase==2)
                startup_screen_update(65,"正在读取稿件和样式","Reading manuscripts and styles");
            else if (phase == NET_READY && storage_phase==4)
                startup_screen_update(90,"稿件存储不可用，连接已就绪","Storage unavailable / Connections ready");
            else if (phase == NET_READY)
                startup_screen_update(90, "无线连接已就绪", "Wi-Fi + Bluetooth ready");
            else if (phase == NET_WIFI_READY)
                startup_screen_update(75, "正在启动蓝牙", "Wi-Fi ready / Starting Bluetooth");
            else if (phase == NET_WIFI_FAILED)
                startup_screen_update(80, "无线暂未就绪", "Display available / Check connection");
            else if (phase == NET_BLE_FAILED)
                startup_screen_update(85, "Wi-Fi 已就绪", "Bluetooth unavailable / Wi-Fi ready");
            else
                startup_screen_update(40, "正在启动无线和设备热点", "Starting wireless and device hotspot");
            if (phase >= NET_READY && !finished_us) finished_us = esp_timer_get_time();
        }
        bool presented=startup_screen_tick();
        if (finished_us && esp_timer_get_time()-finished_us>300000) break;
        /* A submitted frame already waits for the panel. Only idle iterations
         * need a scheduler yield; fixed sleeps also delayed every animation. */
        if(!presented)vTaskDelay(1);
    }
    startup_screen_update(95, "正在准备提词界面", "Preparing the reading screen");
    int64_t preparing_us=esp_timer_get_time();
    ESP_ERROR_CHECK(teleprompter_ui_init());
    /* Reuse half of the previous 600 ms ready hold for the final step. Actual
     * UI construction counts toward it, and the overlay prevents a blank gap. */
    while(esp_timer_get_time()-preparing_us<300000){
        if(!startup_screen_tick())vTaskDelay(1);
    }
    startup_screen_finish();
    teleprompter_ui_resume_motions();
    ESP_LOGI(TAG,"STARTUP_HANDOFF preparing_ms=%" PRId64,(esp_timer_get_time()-preparing_us)/1000);
    ESP_LOGI(TAG, "STARTUP_SCREEN complete; entering teleprompter; stack_free=%u",
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
    atomic_store(&s_ui_running, true);
    bool standby = false;
    bool update_visible=false, update_was_asleep=false;
    bool expired = false;
    uint32_t seen_activity = device_settings_activity_info(&expired);
    device_settings_activity();

    ESP_LOGI(TAG, "P4 teleprompter application started");
    uint32_t samples = 0, slowest_us = 0;
    uint32_t tick_max_us=0,draw_max_us=0,present_max_us=0;
    uint64_t total_us = 0;
    int64_t report_us = esp_timer_get_time();
    while (true) {
        if(factory_reset_pending()){
            if(!update_visible){
                update_visible=true;update_was_asleep=standby;
                prompter_status_t stopped={.playing=false};prompter_control_runtime(TP_PLAYING,&stopped,false);
                display_port_set_standby(false);device_settings_set_asleep(false);
                startup_screen_notice("正在恢复出厂设置","Restoring factory settings");
            }
            if(!startup_screen_tick())vTaskDelay(1);
            continue;
        }
        unsigned update_percent=0;
        tp_update_phase_t update_phase=firmware_update_progress(&update_percent);
        if(update_phase!=TP_UPDATE_IDLE){
            if(!update_visible){
                update_was_asleep=standby;update_visible=true;
                prompter_status_t stopped={.playing=false};
                prompter_control_runtime(TP_PLAYING,&stopped,false);
                display_port_set_standby(false);
                device_settings_set_asleep(false);
                ESP_LOGI(TAG,"UPDATE_SCREEN visible");
            }
            startup_screen_firmware(update_phase,update_percent);
            if(!startup_screen_tick())vTaskDelay(1);
            continue;
        }
        if(update_visible){
            startup_screen_dismiss_notice();update_visible=false;device_settings_activity();
            teleprompter_ui_resume_motions();
            device_settings_set_asleep(update_was_asleep);
            if(update_was_asleep)display_port_set_standby(true);
            else lv_obj_invalidate(lv_screen_active());
            ESP_LOGI(TAG,"UPDATE_SCREEN dismissed");
        }
        uint32_t activity = device_settings_activity_info(&expired);
        prompter_status_t playback;
        prompter_snapshot(&playback, NULL);
        bool active = playback.playing || playback.countdown_remaining_ms > 0;
        bool interacted = activity != seen_activity;
        seen_activity = activity;
        if (active) device_settings_activity();
        if (atomic_load(&s_device_action) == TP_DEVICE_NONE) {
            int expected=TP_DEVICE_NONE;
            bool presence_wake=false;
            if(!active&&device_settings_shutdown_due()&&firmware_update_allows_shutdown()){
                atomic_compare_exchange_strong(&s_device_action,&expected,TP_DEVICE_SHUTDOWN);
            }else if(device_settings_presence_action(&presence_wake)){
                teleprompter_ui_show_guide(!presence_wake);
                if(!presence_wake){prompter_status_t paused={.playing=false};prompter_control_runtime(TP_PLAYING,&paused,false);}
                if(standby)atomic_compare_exchange_strong(&s_device_action,&expected,TP_DEVICE_WAKE);
            }
            else if (standby && interacted && device_settings_presence_allows_wake()) atomic_compare_exchange_strong(&s_device_action,&expected,TP_DEVICE_WAKE);
            /* Before any manuscript is selected, keep the setup QR available
             * even if a library visitor leaves or the idle timer expires. */
            else if (!standby && !active && expired && playback.length>0) atomic_compare_exchange_strong(&s_device_action,&expected,TP_DEVICE_STANDBY);
        }
        tp_device_action_t action = atomic_load(&s_device_action);
        if (action != TP_DEVICE_NONE) {
            /* Consume the activity that requested this action, while preserving
             * any new control arriving during the visible notice. */
            seen_activity = device_settings_activity_info(&expired);
            if (action != TP_DEVICE_WAKE) prompter_control(TP_PLAYING, false, 0, 0, 0);
            display_port_set_standby(false);
            if (action == TP_DEVICE_WAKE) {
                startup_screen_dismiss_notice();
                teleprompter_ui_resume_motions();
                standby = false;
                atomic_store(&s_screen_asleep,false);
                device_settings_set_asleep(false);
                device_settings_activity();
            } else if (action == TP_DEVICE_STANDBY) {
                /* Dim the reading frame, then blank the panel while asleep. */
                startup_screen_dismiss_notice();
                display_port_set_standby(true);
                standby = true;
                atomic_store(&s_screen_asleep,true);
                device_settings_set_asleep(true);
            } else {
                startup_screen_notice(action == TP_DEVICE_SHUTDOWN ? "正在关机" : action == TP_DEVICE_RESTART ? "正在重新启动" :
                                      action == TP_DEVICE_DOWNLOAD ? "正在更新" : "屏幕即将休眠",
                                      action == TP_DEVICE_SHUTDOWN ? "Press P4 RST to turn on" : "Please wait a moment");
                ESP_LOGI(TAG, "DEVICE_NOTICE action=%d visible", action);
                int64_t deadline = esp_timer_get_time() + 1200000;
                bool exiting=false;
                while (esp_timer_get_time() < deadline) {
                    if(!exiting&&deadline-esp_timer_get_time()<=(int64_t)TP_MOTION_EXIT_MS*1000){
                        startup_screen_exit_notice();exiting=true;
                    }
                    if(!startup_screen_tick())vTaskDelay(1);
                }
                if (action == TP_DEVICE_RESTART) {
                    display_port_set_standby(true);
                    display_port_power_off();
                    esp_restart();
                }
                if (action == TP_DEVICE_SHUTDOWN) {
                    /* Block until any library transaction has closed its files, then
                     * keep the recursive lock held until reset. No timed wake. */
                    script_library_shutdown_lock();
                    display_port_set_standby(true);
                    display_port_power_off();
                    (void)esp_wifi_stop();
                    if(atomic_load(&s_network_phase)==NET_READY)(void)nimble_port_stop();
                    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
                    ESP_LOGI(TAG, "SOFTWARE_SHUTDOWN entering deep sleep; restart with P4 RST");
                    esp_deep_sleep_start();
                }
                if (action == TP_DEVICE_DOWNLOAD) {
                    atomic_store(&s_download_notice_done, true);
                    vTaskDelay(pdMS_TO_TICKS(3000));
                }
            }
            atomic_store(&s_device_action, TP_DEVICE_NONE);
        }
        if (standby) { display_port_capture_service(); vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        int64_t started_us = esp_timer_get_time();
        teleprompter_ui_tick();
        int64_t tick_done_us=esp_timer_get_time();
        lv_timer_handler();
        int64_t draw_done_us=esp_timer_get_time();
        bool presented = teleprompter_ui_present();
        display_port_tick();
        uint32_t work_us = (uint32_t)(esp_timer_get_time() - started_us);
        uint32_t tick_us=tick_done_us-started_us,draw_us=draw_done_us-tick_done_us;
        uint32_t present_us=work_us-(uint32_t)(draw_done_us-started_us);
        if(tick_us>tick_max_us)tick_max_us=tick_us;
        if(draw_us>draw_max_us)draw_max_us=draw_us;
        if(present_us>present_max_us)present_max_us=present_us;
        total_us += work_us;
        if (work_us > slowest_us) slowest_us = work_us;
        ++samples;
        if (esp_timer_get_time() - report_us >= 10000000) {
            ESP_LOGI(TAG, "UI_WORK samples=%lu avg_us=%lu max_us=%lu tick_max_us=%lu draw_max_us=%lu present_max_us=%lu stack_free=%u",
                     (unsigned long)samples, (unsigned long)(total_us / samples),
                     (unsigned long)slowest_us,(unsigned long)tick_max_us,(unsigned long)draw_max_us,
                     (unsigned long)present_max_us,(unsigned)uxTaskGetStackHighWaterMark(NULL));
            samples = slowest_us = 0;
            tick_max_us=draw_max_us=present_max_us=0;
            total_us = 0;
            report_us = esp_timer_get_time();
        }
        /* An actual presentation already blocks for DMA and panel completion,
         * allowing the idle task to run. Only idle/subpixel iterations need an
         * extra delay; a fixed delay after every frame misses the next VSYNC. */
        if (!presented) vTaskDelay(1);
    }
}

esp_err_t teleprompter_start_display(void)
{
    if (s_display_attempted) return ESP_ERR_INVALID_STATE;
    s_display_attempted = true;
    /* CFF glyph rasterization nested inside animated LVGL layers exceeds 8 KiB.
     * Keep headroom for large Chinese text, QR, countdown, and transition pages. */
    if (xTaskCreatePinnedToCore(display_task, "display_ui", 24 * 1024, NULL,
                               2, NULL, 1) != pdPASS) {
        s_display_attempted = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool teleprompter_display_can_retry(void)
{
    return !s_display_attempted;
}

void app_main(void)
{
    display_port_boot_dark();
    ESP_ERROR_CHECK(xTaskCreate(usb_download_task, "usb_download", 3072, NULL,
                                5, &s_download_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    const tinyusb_config_t usb_config = TINYUSB_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(tinyusb_driver_install(&usb_config));
    ESP_ERROR_CHECK(esp_register_shutdown_handler(detach_usb_on_restart));

    const tinyusb_config_cdcacm_t cdc_config = {
        .callback_line_coding_changed = usb_line_coding_changed,
    };
    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&cdc_config));
    ESP_ERROR_CHECK(tinyusb_console_init(TINYUSB_CDC_ACM_0));
    ESP_ERROR_CHECK(xTaskCreate(usb_console_task, "usb_console", 4096, NULL, 1, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "USB CDC console ready; connect at 115200 baud; reset_reason=%d", (int)esp_reset_reason());
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(factory_reset_apply_pending());
    ESP_ERROR_CHECK(prompter_state_init());
    ESP_ERROR_CHECK(teleprompter_preview_init());
    ESP_ERROR_CHECK(device_settings_init());
    ESP_ERROR_CHECK(script_library_init());
    ESP_ERROR_CHECK(firmware_update_init());

    if (esp_reset_reason() == ESP_RST_BROWNOUT) {
        /* Keep USB and the control service usable after an observed power dip.
         * A full power cycle retries the screen; do not disable brownout protection. */
        gpio_set_direction(GPIO_NUM_20, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_20, 0);
        gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_5, 0);
        ESP_LOGW(TAG, "POWER_RECOVERY: LCD disabled after brownout; USB/network remain available");
    } else {
        ESP_ERROR_CHECK(teleprompter_start_display());
    }
    ESP_ERROR_CHECK(xTaskCreate(network_task, "network", 8192, NULL,
                                1, &s_network_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    while (true) {
        ESP_LOGI(TAG, "alive; display=%s refreshes=%" PRIu32 " reset_reason=%d",
                 display_port_get_lvgl_display() ? "ready" :
                 (esp_reset_reason() == ESP_RST_BROWNOUT ? "recovery-off" : "initializing"),
                 display_port_get_refresh_count(), (int)esp_reset_reason());
        vTaskDelay(pdMS_TO_TICKS(2000));
        /* Confirm a newly installed slot only after the UI has stayed alive. */
        if(atomic_load(&s_ui_running)&&esp_timer_get_time()>15000000)firmware_update_healthy();
        if(atomic_load(&s_network_phase)==NET_READY)device_settings_refresh_default_names();
        if(atomic_load(&s_network_phase)>=NET_WIFI_READY)wifi_settings_tick();
    }
}
