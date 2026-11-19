#pragma once

/* Set by the transport entry point, never by fields in the request body. */
typedef enum {
    TP_REQUEST_UNKNOWN = 0,
    TP_REQUEST_HTTP,
    TP_REQUEST_BLE,
} tp_request_source_t;
