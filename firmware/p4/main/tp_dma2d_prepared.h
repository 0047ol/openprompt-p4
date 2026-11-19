/* ESP-IDF 6.1 backend ABI; only used by the isolated prepared-buffer adapter. */
#pragma once
#include "esp_private/dma2d.h"
#include "esp_async_color_convert.h"
#define DEFAULT_COLOR_CONVERT_BACKLOG 8
typedef struct async_color_convert_context_t async_color_convert_context_t;

struct async_color_convert_context_t {
    esp_err_t (*convert)(async_color_convert_handle_t ctx,
                         const async_color_convert_request_t *request,
                         async_color_convert_isr_cb_t callback, void *context);
    esp_err_t (*del)(async_color_convert_handle_t ctx);
};

/* Sources must be cache-flushed and immutable until completion. Destinations
 * must have no CPU dirty cache lines and must not be accessed by CPU in flight.
 * The ordinary SDK API continues to synchronize buffers normally. */
esp_err_t tp_dma2d_prepared_install(const async_color_convert_config_t *config,
                                   async_color_convert_handle_t *handle);
