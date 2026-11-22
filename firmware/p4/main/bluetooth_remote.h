#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
void bluetooth_remote_init(void);
void bluetooth_remote_sync(uint8_t address_type);
void bluetooth_remote_reset(void);
unsigned bluetooth_remote_connections(void);
void bluetooth_remote_pause(bool paused);
cJSON *bluetooth_remote_request(const cJSON *request);
/* Browser links remain independent of the physical HID input device. */
unsigned bluetooth_control_browser_connections(void);
void bluetooth_control_remote_capacity_changed(void);
