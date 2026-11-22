#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t device_settings_init(void);
esp_err_t device_settings_json(char *buffer, size_t size);
uint32_t device_settings_revision(void);
esp_err_t device_settings_update(const char *json);
void device_settings_boot_names(char *ap, size_t ap_size, char *ble, size_t ble_size);
/* Called once after remote Wi-Fi init, before either radio starts advertising. */
void device_settings_radio_identity(const uint8_t mac[6]);
void device_settings_display_name(char *name, size_t size);
void device_settings_activity(void);
uint32_t device_settings_activity_info(bool *expired);
void device_settings_set_asleep(bool asleep);
bool device_settings_asleep(void);
void device_settings_client_seen(const char *id);
void device_settings_ble_connected(bool connected);
void device_settings_ble_count(unsigned count);
void device_settings_library_view(int *layout,int *sort,uint32_t *revision);
esp_err_t device_settings_presence(const char *id, bool reading, bool bluetooth);
bool device_settings_playback_allowed(const char *client);
bool device_settings_playback_patch_allowed(const char *text,const char *client);
bool device_settings_presence_action(bool *wake);
bool device_settings_presence_allows_wake(void);
bool device_settings_presence_managed(void);
bool device_settings_shutdown_due(void);
void device_settings_presence_route(const char *id,const char *peer,bool hotspot,int ble_handle);
void device_settings_presence_disconnect(const char *peer,int transport);
/* Ignore a delayed close from a socket replaced by a newer connection. */
void device_settings_presence_socket(const char *id,uint32_t token,bool open);
/* transport: 0=all network, 1=hotspot, 2=router, 3=BLE; peer optionally narrows it. */
/* 0 Simplified Chinese, 1 English. Read on the UI task before drawing. */
void device_settings_ui(int *language, bool *show_info);
void device_settings_boot_password(char *password, size_t size);
void device_settings_refresh_default_names(void);
void device_settings_ble_name_applied(const char *name);

enum {
    TP_BAR_TIME=1, TP_BAR_DATE=2, TP_BAR_STATE=4, TP_BAR_NEXT=8,
    TP_BAR_SPEED=16, TP_BAR_FPS=32, TP_BAR_OFFSET=64, TP_BAR_BUFFER=128,
    TP_BAR_JITTER=256, TP_BAR_RENDER=512, TP_BAR_PROGRESS=1024,
    TP_BAR_LOOP=2048, TP_BAR_MEMORY=4096, TP_BAR_IP=8192,
    TP_BAR_FONT=16384, TP_BAR_BRIGHTNESS=32768,
    TP_BAR_MAC=65536, TP_BAR_NAME=131072, TP_BAR_LETTER=262144,
    TP_BAR_LINE=524288, TP_BAR_MARGIN=1048576, TP_BAR_COUNTDOWN=2097152,
    TP_BAR_BITRATE=4194304, TP_BAR_PREVIOUS=8388608,
    TP_BAR_DRAWN=16777216, TP_BAR_LATE=33554432,
    TP_BAR_TIMECODE=67108864, TP_BAR_ALL=134217727
};
typedef struct {
    uint32_t fields, header_height, footer_height;
    bool header, footer;
    uint32_t header_font_size, footer_font_size;
} device_bars_t;
void device_settings_bars(device_bars_t *bars);
#define TP_BAR_FOOTER_MAX 6
#define TP_BAR_RETIRED_FIELDS (TP_BAR_OFFSET|TP_BAR_IP|TP_BAR_MAC)
#define TP_BAR_HEADER_FIELDS (TP_BAR_TIME|TP_BAR_DATE|TP_BAR_STATE|TP_BAR_NEXT|TP_BAR_LOOP|TP_BAR_NAME|TP_BAR_PREVIOUS|TP_BAR_TIMECODE)
void device_settings_clock_hint(int offset_minutes);
int device_settings_clock_offset(void);
