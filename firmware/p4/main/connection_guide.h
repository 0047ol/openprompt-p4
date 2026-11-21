#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    TP_GUIDE_JOIN, TP_GUIDE_WIFI_CONNECTING,
    TP_GUIDE_LAN, TP_GUIDE_BLUETOOTH, TP_GUIDE_RECONNECT, TP_GUIDE_READY
} tp_guide_stage_t;
typedef struct {
    bool hotspot, wifi_enabled, wifi_connected, wifi_failed;
    bool portal_open, controller, bluetooth_selected;
    unsigned hotspot_clients;
    char ssid[33], address[16], hotspot_address[16], local_domain[70];
} tp_guide_input_t;
typedef struct {
    tp_guide_stage_t stage, pending;
    uint32_t pending_at;
    bool initialized;
} tp_guide_tracker_t;
typedef struct { unsigned step; const char *title[2], *body[2]; bool qr, centered, loading; } tp_guide_copy_t;
#define TP_GUIDE_PEER_CAPACITY 10
typedef struct { unsigned char mac[TP_GUIDE_PEER_CAPACITY][6]; bool used[TP_GUIDE_PEER_CAPACITY]; } tp_guide_peers_t;

/* Pure state selection: no radio calls, LVGL calls, or control-slot claims. */
tp_guide_stage_t tp_guide_stage(tp_guide_tracker_t *tracker, const tp_guide_input_t *input, uint32_t now_ms);
tp_guide_stage_t tp_guide_content_stage(tp_guide_stage_t stage, const tp_guide_input_t *input);
/* Animate only a real step change while the guide is already on screen. */
bool tp_guide_animate_step(bool presented, bool initialized, tp_guide_stage_t before, tp_guide_stage_t after);
bool tp_guide_wifi_payload(char *out, size_t capacity, const char *name, const char *password);
bool tp_guide_format_body(char *out, size_t capacity, const char *format,
                          const char *hotspot, const char *ssid, const char *bluetooth, const char *url);
void tp_guide_peer_update(tp_guide_peers_t *peers, const unsigned char mac[6], bool joined);
unsigned tp_guide_peer_count(const tp_guide_peers_t *peers);
