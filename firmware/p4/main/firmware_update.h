#pragma once
#include "esp_err.h"
#include "cJSON.h"
esp_err_t firmware_update_init(void);
void firmware_update_healthy(void);
bool firmware_update_allows_shutdown(void);
bool firmware_update_allows_restart(void);
bool firmware_update_in_progress(void);
/* Nonblocking display snapshot; safe while flash writes hold the update lock. */
typedef enum { TP_UPDATE_IDLE, TP_UPDATE_CHECKING, TP_UPDATE_WRITING, TP_UPDATE_RESTARTING } tp_update_phase_t;
tp_update_phase_t firmware_update_progress(unsigned *percent);
cJSON *firmware_update_request(const cJSON *request);
