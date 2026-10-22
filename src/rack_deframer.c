// =============================================================================
// URTC-SMART-RACK Firmware - Stream deframer for the serial link: rack_deframer.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "rack_deframer.h"

#include <stddef.h>

// Throws away the first `n` bytes held (loops, not memmove: this file also builds for a
// target with no C library).
static void drop_front(rack_deframer_t *d, uint8_t n)
{
    if (n >= d->count) {
        d->stats.dropped_bytes += d->count;
        d->count = 0;
        return;
    }
    d->stats.dropped_bytes += n;
    for (uint8_t i = n; i < d->count; i++) {
        d->buf[i - n] = d->buf[i];
    }
    d->count = (uint8_t)(d->count - n);
}

// Index of the next start byte after the first one, or the number of bytes held when there is none.
static uint8_t next_start(const rack_deframer_t *d)
{
    uint8_t skip = 1u;
    while (skip < d->count && d->buf[skip] != PROTOCOL_SOF) {
        skip++;
    }
    return skip;
}

// Removes the first byte of a candidate that turned out not to be a frame and skips ahead to
// the next start byte already held.
static void resync(rack_deframer_t *d)
{
    d->stats.resyncs++;
    drop_front(d, next_start(d));
}

// One look at what is held. Returns the length of a frame copied to `out`, or 0.
static uint8_t extract(rack_deframer_t *d, uint8_t *out)
{
    while (d->count > 0u) {
        if (d->buf[0] != PROTOCOL_SOF) {
            drop_front(d, next_start(d));
            continue;
        }
        if (d->count >= 2u && d->buf[1] != PROTOCOL_VERSION) {
            d->stats.bad_headers++;
            resync(d);
            continue;
        }
        if (d->count < 5u) {
            return 0; // header not complete yet
        }
        const uint8_t payload_len = d->buf[4];
        if (payload_len > PROTOCOL_MAX_PAYLOAD) {
            d->stats.bad_headers++;
            resync(d);
            continue;
        }
        const uint8_t total = (uint8_t)(6u + payload_len);
        if (d->count < total) {
            return 0; // still waiting for the rest of the frame
        }
        protocol_frame_t decoded;
        const protocol_status_t status = protocol_parse_frame(d->buf, total, &decoded);
        if (status != PROTOCOL_OK) {
            if (status == PROTOCOL_ERR_CRC_MISMATCH) {
                d->stats.crc_errors++;
            } else {
                d->stats.bad_headers++;
            }
            resync(d);
            continue;
        }
        for (uint8_t i = 0; i < total; i++) {
            out[i] = d->buf[i];
        }
        d->stats.frames_ok++;
        // The frame is consumed, not dropped: keep the counter of dropped bytes as it was.
        const uint32_t dropped = d->stats.dropped_bytes;
        drop_front(d, total);
        d->stats.dropped_bytes = dropped;
        return total;
    }
    return 0;
}

void rack_deframer_init(rack_deframer_t *d, uint32_t timeout_ms)
{
    if (d == NULL) {
        return;
    }
    d->count = 0;
    d->last_byte_ms = 0;
    d->timeout_ms = (timeout_ms == 0u) ? RACK_DEFRAMER_DEFAULT_TIMEOUT_MS : timeout_ms;
    d->stats.frames_ok = 0;
    d->stats.crc_errors = 0;
    d->stats.bad_headers = 0;
    d->stats.resyncs = 0;
    d->stats.timeouts = 0;
    d->stats.dropped_bytes = 0;
}

uint8_t rack_deframer_push(rack_deframer_t *d, uint8_t byte, uint32_t now_ms, uint8_t *out_frame)
{
    if (d == NULL || out_frame == NULL) {
        return 0;
    }
    // Unsigned subtraction: correct across the wrap of the millisecond counter.
    if (d->count > 0u && (uint32_t)(now_ms - d->last_byte_ms) > d->timeout_ms) {
        d->stats.timeouts++;
        d->stats.dropped_bytes += d->count;
        d->count = 0;
    }
    d->last_byte_ms = now_ms;
    if (d->count >= PROTOCOL_MAX_FRAME_SIZE) {
        // Cannot happen (a candidate is resolved before it grows past the longest frame), but a
        // full buffer must never be written past.
        drop_front(d, 1u);
    }
    d->buf[d->count++] = byte;
    return extract(d, out_frame);
}

uint8_t rack_deframer_next(rack_deframer_t *d, uint8_t *out_frame)
{
    if (d == NULL || out_frame == NULL) {
        return 0;
    }
    return extract(d, out_frame);
}

void rack_deframer_reset(rack_deframer_t *d)
{
    if (d != NULL) {
        d->count = 0;
    }
}
