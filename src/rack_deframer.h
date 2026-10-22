// =============================================================================
// URTC-SMART-RACK Firmware - Stream deframer for the serial link: rack_deframer.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// A serial link delivers a stream of bytes, not frames: a byte can be lost, a frame can
// arrive in pieces, noise can come between two frames. protocol_parse_frame() decodes one
// frame that somebody already delimited; this module is the somebody. It looks for the start
// byte, collects the bytes the header says belong to the frame, and hands over a frame only
// when its CRC is right. When the bytes do not fit (wrong version, impossible length, bad
// CRC) it throws away the first byte of the candidate and looks again from the next start
// byte inside what it already holds, so the frame that follows a damaged one is not lost
// with it. A frame that stops half-way is dropped after an inter-byte timeout, so a start
// byte that was noise cannot swallow the next real frame.
//
// Pure logic against a caller-supplied millisecond clock, like the rest of the rack. Call it
// from ONE context (the main loop, fed from the ring buffer a receive interrupt fills).
//
//   len = rack_deframer_push(&rx, byte, now_ms, frame);   // for every received byte
//   while (len != 0) { handle(frame, len); len = rack_deframer_next(&rx, frame); }
#ifndef RACK_DEFRAMER_H
#define RACK_DEFRAMER_H

#include <stdbool.h>
#include <stdint.h>

#include "protocol.h"

// Longest silence inside a frame. At 115200 baud a whole frame lasts under 2 ms, so 20 ms
// leaves room for a loaded main loop and still frees the receiver quickly.
#define RACK_DEFRAMER_DEFAULT_TIMEOUT_MS 20u

typedef struct {
    uint32_t frames_ok;       // frames handed over
    uint32_t crc_errors;      // candidates that had the right shape and the wrong CRC
    uint32_t bad_headers;     // candidates with an unsupported version or an impossible length
    uint32_t resyncs;         // times the deframer restarted from a later start byte
    uint32_t timeouts;        // half frames dropped by the inter-byte timeout
    uint32_t dropped_bytes;   // bytes thrown away for any reason
} rack_deframer_stats_t;

typedef struct {
    uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
    uint8_t count;
    uint32_t last_byte_ms;
    uint32_t timeout_ms;
    rack_deframer_stats_t stats;
} rack_deframer_t;

// A `timeout_ms` of 0 is replaced by RACK_DEFRAMER_DEFAULT_TIMEOUT_MS (a zero timeout would
// drop every frame that is not delivered in a single instant).
void rack_deframer_init(rack_deframer_t *d, uint32_t timeout_ms);

// Takes one received byte. Returns the length of a complete, CRC-valid frame, copied into
// `out_frame` (room for PROTOCOL_MAX_FRAME_SIZE bytes), or 0 when no frame is complete yet.
uint8_t rack_deframer_push(rack_deframer_t *d, uint8_t byte, uint32_t now_ms, uint8_t *out_frame);

// Looks for another frame among the bytes already held, without a new byte. After a
// resynchronisation the bytes that were kept can contain a whole frame; call this until it
// returns 0 after every frame that push or next returned.
uint8_t rack_deframer_next(rack_deframer_t *d, uint8_t *out_frame);

// Forgets the bytes held and keeps the counters (a link that was reset starts a new frame).
void rack_deframer_reset(rack_deframer_t *d);

#endif // RACK_DEFRAMER_H
