#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

/* Fixed numeric stages only: never pass document text, names or credentials. */
typedef enum { TP_TRACE_BOOT, TP_TRACE_STORAGE, TP_TRACE_UPDATE, TP_TRACE_POWER,
               TP_TRACE_SD_TEST, TP_TRACE_COUNT } tp_trace_domain_t;
void crash_log_early_init(void);
void crash_log_init(void);
void crash_log_stage(tp_trace_domain_t domain, unsigned stage);
bool crash_log_pending(void);
unsigned crash_log_last_id(void);
unsigned crash_log_count(void);
/* Caller owns the result. Acknowledgement retains the history. */
cJSON *crash_log_request(const cJSON *request);
