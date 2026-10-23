// =============================================================================
// URTC-SMART-RACK Firmware - Rack command protocol: protocol.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "protocol.h"
#include "urtc_crc8.h"   // the checksum shared with the vision sensor link (copied from URTC-COMMON)
#include "urtc_frame.h"  // the frame layer shared with the vision sensor link (copied from URTC-COMMON)
#include <stddef.h>

// The rack frame is the shared layer with the length byte at index 4 (SEQ and CMD sit before it).
static const urtc_frame_format_t RACK_FRAME = { PROTOCOL_SOF, PROTOCOL_VERSION, 4u, PROTOCOL_MAX_PAYLOAD };

// The statuses of the shared layer have the same order as ours; the cast below relies on it.
_Static_assert((int)URTC_FRAME_OK == (int)PROTOCOL_OK, "status order: ok");
_Static_assert((int)URTC_FRAME_INCOMPLETE == (int)PROTOCOL_ERR_INCOMPLETE, "status order: incomplete");
_Static_assert((int)URTC_FRAME_BAD_SOF == (int)PROTOCOL_ERR_BAD_SOF, "status order: bad start byte");
_Static_assert((int)URTC_FRAME_UNSUPPORTED_VERSION == (int)PROTOCOL_ERR_UNSUPPORTED_VERSION, "status order: unsupported version");
_Static_assert((int)URTC_FRAME_LENGTH_OUT_OF_RANGE == (int)PROTOCOL_ERR_LENGTH_OUT_OF_RANGE, "status order: length out of range");
_Static_assert((int)URTC_FRAME_CRC_MISMATCH == (int)PROTOCOL_ERR_CRC_MISMATCH, "status order: CRC mismatch");

uint8_t protocol_crc8(const uint8_t *data, uint8_t len)
{
    return urtc_crc8(data, len);
}

protocol_status_t protocol_parse_frame(const uint8_t *buf, uint8_t buf_len, protocol_frame_t *out_frame)
{
    if (buf == NULL || out_frame == NULL) {
        return PROTOCOL_ERR_INCOMPLETE;
    }
    // Start byte, version, length, completeness and CRC are decided by the shared frame layer, in that order.
    uint8_t total_len = 0;
    const urtc_frame_status_t status = urtc_frame_validate(&RACK_FRAME, buf, buf_len, &total_len);
    if (status != URTC_FRAME_OK) {
        return (protocol_status_t)status;
    }
    const uint8_t version = buf[1];
    const uint8_t len = buf[4];

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
    urtc_frame_start(&RACK_FRAME, out_buf, payload_len);
    out_buf[2] = seq;
    out_buf[3] = cmd;
    for (uint8_t i = 0; i < payload_len; i++) {
        out_buf[5u + i] = payload[i];
    }
    return urtc_frame_seal(&RACK_FRAME, out_buf, payload_len);
}
