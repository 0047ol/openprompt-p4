#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t startup_screen_init(void);
void startup_screen_update(int progress, const char *status, const char *detail);
void startup_screen_finish(void);
void startup_screen_notice(const char *message, const char *detail);
void startup_screen_dismiss_notice(void);
void startup_screen_firmware(unsigned stage, unsigned percent);
void startup_screen_power(unsigned percent,const char *status,const char *detail);
void startup_screen_crash(void);
bool startup_screen_tick(void);
void startup_screen_exit_notice(void);
