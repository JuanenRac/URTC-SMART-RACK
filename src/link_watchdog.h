// =============================================================================
// URTC-SMART-RACK Firmware - Link timeout + command idempotency: link_watchdog.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// Real timeout and idempotency tracking for the rack link - the promotion
// audit's own "timeout, idempotencia... y estado seguro al arrancar o
// perder enlace". Pure logic against a caller-supplied millisecond clock
// (no SysTick/RTC access), so it's real and testable on the host without
// a real link or board.
#ifndef LINK_WATCHDOG_H
#define LINK_WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

// One idempotency slot per possible tool ID (see tool_id.h's
// TOOL_ID_BITS) - TOOL_ID_NONE (all tool IDs decode to at most this many
// values) is never actually tracked, see link_watchdog_accept_sequence().
#define LINK_WATCHDOG_TOOL_SLOTS 32u

typedef struct {
    uint32_t last_rx_ms;
    uint32_t timeout_ms;
    bool has_received_any_frame;
    uint8_t last_seq_by_tool_id[LINK_WATCHDOG_TOOL_SLOTS];
    bool has_seq_by_tool_id[LINK_WATCHDOG_TOOL_SLOTS];
} link_watchdog_t;

void link_watchdog_init(link_watchdog_t *lw, uint32_t timeout_ms);

// Must be called once for every real, CRC-valid frame received (see
// protocol_parse_frame()) - marks the link alive as of `now_ms`.
void link_watchdog_note_frame_received(link_watchdog_t *lw, uint32_t now_ms);

// True once `timeout_ms` has really elapsed since the last real frame -
// or, just as importantly, if no frame has EVER arrived (real "estado
// seguro al arrancar": a link that has never been proven alive must be
// treated exactly like one that just died, not silently assumed fine).
// A caller must treat true here as "drop every actuator to its safe
// state", not "keep whatever was last commanded".
bool link_watchdog_is_link_lost(const link_watchdog_t *lw, uint32_t now_ms);

// Real idempotency/anti-reorder check for one continuous session: a
// sequence number that is not genuinely newer than the last one accepted
// for this exact tool_id (a real duplicate resend, or a stale/reordered
// older frame - see RACK-01's own comment in link_watchdog.c) is rejected,
// not just an exact repeat of the immediately-previous value. Correctly
// handles real 8-bit wraparound (255 -> 0). Only a real, present tool_id
// (see tool_id.h) is ever tracked - an absent/invalid slot always returns
// false rather than silently allocating tracking state for it. This is
// NOT cryptographic anti-replay protection on its own - see
// link_watchdog_reset_all_sequences() for the real session-boundary
// handling a bare sequence counter cannot provide by itself.
bool link_watchdog_accept_sequence(link_watchdog_t *lw, uint8_t tool_id, uint8_t seq);

// RACK-01: clears every per-tool "last sequence seen" slot - call this
// whenever a real, independent signal (link_watchdog_is_link_lost()
// having been true just before this frame) means the previous session's
// sequence state can no longer be trusted as a comparison baseline for a
// fresh one. rack_link_process_frame() already does this automatically;
// exposed here for any other real caller with its own session-boundary
// signal (e.g. a future protocol revision with an explicit boot/session
// counter).
void link_watchdog_reset_all_sequences(link_watchdog_t *lw);

#endif // LINK_WATCHDOG_H
