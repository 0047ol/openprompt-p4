#pragma once
#include "prompter_state.h"
#include "request_source.h"

/* Saved scripts live in the selected SD/internal library. Responses are owned by caller. */
esp_err_t script_library_init(void);
/* Nonblocking startup snapshot: 0 waiting, 1 mounting, 2 reading, 3 ready, 4 unavailable. */
unsigned script_library_startup_stage(void);
void script_library_startup_begin(void);
bool script_library_maintenance(void);
/* Physical SD reservation for SD firmware updates; never redirects to internal storage. */
esp_err_t script_library_storage_lock(void);
/* Lock only if storage is idle; never mount or modify the card. */
esp_err_t script_library_try_idle_lock(void);
void script_library_storage_unlock(void);
/* Drain all library writes and keep them blocked until device shutdown. */
void script_library_shutdown_lock(void);
void script_library_advance(uint32_t script_revision);
char *script_library_request(const char *request, tp_request_source_t source);
uint32_t script_library_revision(void);
/* Cached by the storage worker; never reads SD or waits for storage from UI. */
void script_library_next_title(char *title,size_t size);
void script_library_previous_title(char *title,size_t size);
esp_err_t script_library_control(unsigned mask,const prompter_status_t *values);
