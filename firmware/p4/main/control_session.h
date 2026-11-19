#pragma once
#include <stdbool.h>
#include "esp_http_server.h"
/* Preferred guide address only; hotspot, router and BLE coexist. */
typedef enum { TP_LINK_HOTSPOT, TP_LINK_LAN } tp_link_mode_t;
tp_link_mode_t control_session_mode(void);
const char *control_session_mode_name(void);
void control_session_set_mode(tp_link_mode_t mode);
bool control_session_claim(const char *peer);
void control_session_release(const char *peer);
bool control_session_http(httpd_req_t *req);
/* Interface that actually received this HTTP request, independent of guide mode. */
bool control_session_http_hotspot(httpd_req_t *req);
bool control_session_socket(httpd_handle_t server, int fd, bool renew);
bool control_session_ble(void);
/* Legacy portal entry points; interactive portal has been removed. */
bool control_session_portal_open(void);
/* Release router peers after link failure without affecting hotspot or BLE. */
void control_session_network_failed(bool failed);
bool control_session_network_available(httpd_req_t *req);
bool control_session_network_controller(httpd_req_t *req);
/* Language observation never reserves a controller. */
bool control_session_portal_language(httpd_req_t *req);
bool control_session_portal_available(httpd_req_t *req, bool reserve);
bool control_session_portal_http(httpd_req_t *req);
void control_session_lan_ready(void);
void control_session_console_http(httpd_req_t *req);
void control_session_console_ble(void);
bool control_session_connected(void);
bool control_session_bluetooth_connected(void);
bool control_session_network_connected(void);
bool control_session_hotspot_controller(void);
bool control_session_ble_peer(unsigned handle,bool ready);
void control_session_ble_release(unsigned handle);
void control_session_route_presence(httpd_req_t *req,const char *client);
void control_session_network_disconnected(bool hotspot);
