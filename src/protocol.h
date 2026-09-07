// =============================================================================
// URTC-SMART-RACK Firmware - Rack command protocol: protocol.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// Real wire framing for the host<->rack link - the promotion audit's own
// "definir protocolo de rack con version, framing, CRC/checksum...; no
// dejar la interpretacion en el loop MCU". Pure byte-buffer logic, no
// UART/CAN peripheral access - real and testable on the host today, wired
// to a real transport once one exists (see main.c's own honest note that
// no PCB/CAN transceiver exists yet for this board).
//
//   byte:   0     1        2      3      4      5..5+len-1   5+len
//         [SOF][VERSION] [SEQ] [CMD] [LEN] [PAYLOAD...]     [CRC8]
//
// CRC8 covers everything from SOF through the last payload byte.
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTOCOL_SOF 0xA5u
#define PROTOCOL_VERSION 1u
#define PROTOCOL_MAX_PAYLOAD 16u
// SOF + VERSION + SEQ + CMD + LEN + payload + CRC
#define PROTOCOL_MAX_FRAME_SIZE (5u + PROTOCOL_MAX_PAYLOAD + 1u)

typedef enum {
    PROTOCOL_OK = 0,
    PROTOCOL_ERR_INCOMPLETE,             // fewer bytes than the frame needs - a real caller should keep buffering, not treat this as corrupt
    PROTOCOL_ERR_BAD_SOF,
    PROTOCOL_ERR_UNSUPPORTED_VERSION,
    PROTOCOL_ERR_LENGTH_OUT_OF_RANGE,
    PROTOCOL_ERR_CRC_MISMATCH,
} protocol_status_t;

typedef struct {
    uint8_t version;
    uint8_t seq;
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
} protocol_frame_t;

// Real CRC-8 (polynomial 0x07, the common SMBus/"CRC-8" variant) over
// `len` bytes starting at `data` - the same checksum both
// protocol_parse_frame() and protocol_encode_frame() use, exposed
// separately so a test/simulator can compute the *correct* value and then
// deliberately write a wrong one to prove a corrupted frame is rejected.
uint8_t protocol_crc8(const uint8_t *data, uint8_t len);

// Real parser: decodes exactly one already-delimited frame (`buf_len`
// bytes) into `out_frame`. Returns PROTOCOL_OK only when the SOF,
// protocol version, declared length and CRC are all real and consistent -
// a frame that fails any one of those checks is never partially trusted,
// `out_frame` is left untouched on any non-OK status.
protocol_status_t protocol_parse_frame(const uint8_t *buf, uint8_t buf_len, protocol_frame_t *out_frame);

// Real encoder: the inverse of protocol_parse_frame(), writing a valid,
// correctly-checksummed frame into `out_buf` (which must have room for at
// least 5 + payload_len + 1 bytes). Returns the real number of bytes
// written, or 0 if payload_len exceeds PROTOCOL_MAX_PAYLOAD.
uint8_t protocol_encode_frame(uint8_t seq, uint8_t cmd, const uint8_t *payload, uint8_t payload_len, uint8_t *out_buf);

#endif // PROTOCOL_H
