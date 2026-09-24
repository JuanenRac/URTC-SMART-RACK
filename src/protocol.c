// =============================================================================
// URTC-SMART-RACK Firmware - Rack command protocol: protocol.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "protocol.h"
#include <stddef.h>

uint8_t protocol_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00u;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8u; bit++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

protocol_status_t protocol_parse_frame(const uint8_t *buf, uint8_t buf_len, protocol_frame_t *out_frame)
{
    if (buf == NULL || out_frame == NULL) {
        return PROTOCOL_ERR_INCOMPLETE;
    }
    // Smallest real frame is a zero-payload one: SOF+VERSION+SEQ+CMD+LEN+CRC.
    if (buf_len < 6u) {
        return PROTOCOL_ERR_INCOMPLETE;
    }
    if (buf[0] != PROTOCOL_SOF) {
        return PROTOCOL_ERR_BAD_SOF;
    }
    uint8_t version = buf[1];
    if (version != PROTOCOL_VERSION) {
        return PROTOCOL_ERR_UNSUPPORTED_VERSION;
    }
    uint8_t len = buf[4];
    if (len > PROTOCOL_MAX_PAYLOAD) {
        return PROTOCOL_ERR_LENGTH_OUT_OF_RANGE;
    }
    uint8_t header_and_payload_len = (uint8_t)(5u + len);
    uint8_t total_len = (uint8_t)(header_and_payload_len + 1u);
    if (buf_len < total_len) {
        return PROTOCOL_ERR_INCOMPLETE;
    }
    uint8_t computed_crc = protocol_crc8(buf, header_and_payload_len);
    uint8_t received_crc = buf[header_and_payload_len];
    if (computed_crc != received_crc) {
        return PROTOCOL_ERR_CRC_MISMATCH;
    }

    out_frame->version = version;
    out_frame->seq = buf[2];
    out_frame->cmd = buf[3];
    out_frame->len = len;
    for (uint8_t i = 0; i < len; i++) {
        out_frame->payload[i] = buf[5u + i];
    }
    // `payload` is a fixed PROTOCOL_MAX_PAYLOAD-byte array - only the
    // first `len` of those bytes are real wire content, but a caller
    // holding this protocol_frame_t as an uninitialized local (as
    // rack_link.c's own rack_link_process_frame() does) has no way to know
    // that on its own. A real, validly-framed zero-payload frame (len ==
    // 0 - see this file's own header comment on the smallest legal frame)
    // used to leave every byte of `payload` as whatever undefined stack
    // garbage the caller's local happened to hold, which a caller reading
    // payload[0] without first checking `len` (the exact bug this note
    // documents) turned into a nondeterministic, memory-unsafe "tool_id".
    // Zeroing the unused tail here means that mistake is merely wrong
    // (reads a well-defined 0), never undefined - real safety net for any
    // future caller, on top of the correct fix already in rack_link.c
    // (checking `len` before ever reading payload[0]).
    for (uint8_t i = len; i < PROTOCOL_MAX_PAYLOAD; i++) {
        out_frame->payload[i] = 0u;
    }
    return PROTOCOL_OK;
}

uint8_t protocol_encode_frame(uint8_t seq, uint8_t cmd, const uint8_t *payload, uint8_t payload_len, uint8_t *out_buf)
{
    if (out_buf == NULL || payload_len > PROTOCOL_MAX_PAYLOAD) {
        return 0;
    }
    if (payload_len > 0u && payload == NULL) {
        return 0;
    }
    out_buf[0] = PROTOCOL_SOF;
    out_buf[1] = PROTOCOL_VERSION;
    out_buf[2] = seq;
    out_buf[3] = cmd;
    out_buf[4] = payload_len;
    for (uint8_t i = 0; i < payload_len; i++) {
        out_buf[5u + i] = payload[i];
    }
    uint8_t header_and_payload_len = (uint8_t)(5u + payload_len);
    out_buf[header_and_payload_len] = protocol_crc8(out_buf, header_and_payload_len);
    return (uint8_t)(header_and_payload_len + 1u);
}
