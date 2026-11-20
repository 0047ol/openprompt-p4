#pragma once
#include "esp_http_server.h"

/* A manually requested snapshot; never streams or polls the panel. */
esp_err_t display_capture_get(httpd_req_t *request);
