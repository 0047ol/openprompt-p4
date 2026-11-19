#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TP_PROTOCOL_VERSION 1U
#define TP_MAX_TEXT_BYTES 4096U

typedef enum {
    TP_MSG_HELLO = 1,
    TP_MSG_SCRIPT_SET = 2,
    TP_MSG_SCRIPT_GET = 3,
    TP_MSG_CONTROL = 4,
    TP_MSG_STATUS = 5,
    TP_MSG_ACK = 6,
} tp_message_type_t;

typedef enum {
    TP_CONTROL_PAUSE = 1,
    TP_CONTROL_RESUME = 2,
    TP_CONTROL_REWIND = 3,
    TP_CONTROL_FORWARD = 4,
    TP_CONTROL_SET_SPEED = 5,
    TP_CONTROL_SET_FONT_SIZE = 6,
} tp_control_code_t;

typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t payload_length;
    uint32_t sequence;
} tp_frame_header_t;

uint16_t tp_crc16(const uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif
