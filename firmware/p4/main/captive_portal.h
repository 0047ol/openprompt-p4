#pragma once
#include "esp_http_server.h"

esp_err_t captive_portal_start(httpd_handle_t server);
/* Register legacy redirects only; captive portal advertising is disabled. */
