#pragma once
void wifi_settings_tick(void);
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "connection_guide.h"
#include "request_source.h"

esp_err_t wifi_settings_init(void);
void wifi_settings_start(void);
esp_err_t wifi_settings_register(httpd_handle_t server);
void wifi_settings_address(char *out, size_t capacity);
void wifi_settings_hotspot_address(char *out, size_t capacity);
void wifi_settings_device_mac(char *out, size_t capacity);
void wifi_settings_guide_snapshot(tp_guide_input_t *out);
esp_err_t wifi_settings_use_hotspot(void);
void wifi_settings_show_bluetooth(bool enabled);
bool wifi_settings_bluetooth_guide(void);
bool wifi_settings_hotspot_enabled(void);
void wifi_settings_confirm_lan(httpd_req_t *req, const char *client);
typedef enum { WIFI_PORTAL_NONE, WIFI_PORTAL_WIFI, WIFI_PORTAL_BLUETOOTH } wifi_portal_mode_t;
void wifi_settings_portal_mode(wifi_portal_mode_t mode);
void wifi_settings_portal_status(const char **phase, int *remaining_ms);
cJSON *wifi_settings_request(const cJSON *request, tp_request_source_t source);

/* Asynchronous transport handoff; response/QR is delivered before the old link closes. */
esp_err_t wifi_settings_switch_mode(const char *mode);
esp_err_t wifi_settings_refresh_ap_name(const char *name);
