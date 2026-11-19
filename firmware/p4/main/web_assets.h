#pragma once
#include <stdbool.h>
#include "esp_http_server.h"
typedef enum { TP_ASSET_EDITOR, TP_ASSET_FONT } tp_web_asset_t;
esp_err_t web_asset_send(httpd_req_t *req,tp_web_asset_t asset);
esp_err_t web_assets_init(void);
bool web_assets_busy(void);
esp_err_t web_asset_send_async(httpd_req_t *req,tp_web_asset_t asset);
