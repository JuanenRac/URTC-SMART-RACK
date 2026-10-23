// =============================================================================
// URTC-SMART-RACK Firmware - Link timeout + command idempotency: link_watchdog.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// Real timeout and idempotency tracking for the rack link - the review's
// own "timeout, idempotencia... y estado seguro al arrancar o
// perder enlace". Pure logic against a caller-supplied millisecond clock
// (no SysTick/RTC access), so it's real and testable on the host without
// a real link or board.
#ifndef LINK_WATCHDOG_H
#define LINK_WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

#include "tool_id.h"
#include "urtc_rate_limiter.h" // the minimum interval between accepted messages shared with the vision sensor link (copied from URTC-COMMON)

// One idempotency slot per possible tool ID (see tool_id.h's
// TOOL_ID_BITS) - TOOL_ID_NONE (all tool IDs decode to at most this many
// values) is never actually tracked, see link_watchdog_accept_sequence().
#define LINK_WATCHDOG_TOOL_SLOTS (1u << TOOL_ID_BITS)

// Timeout used when link_watchdog_init() is given 0: a zero timeout would make
// the link look lost at every instant (and wipe the idempotency state on every
// frame), so it is not a configuration that can be honoured. 250 ms is the
// communication watchdog of the URTC tool heads.
#define LINK_WATCHDOG_DEFAULT_TIMEOUT_MS 250u

// The least time between two commands accepted for the same tool: a sender that floods the link is throttled instead of obeyed at whatever rate it sends. 20 ms is far
// below how often a real sender repeats a command (a few times a second) and far above a flood. A build can change it with -DRACK_COMMAND_MIN_INTERVAL_MS=...; at run time
// link_watchdog_set_command_interval() takes the value, and 0 switches the limit off.
#ifndef RACK_COMMAND_MIN_INTERVAL_MS
#define RACK_COMMAND_MIN_INTERVAL_MS 20u
#endif

typedef struct {
    uint32_t last_rx_ms;
    uint32_t timeout_ms;
    bool has_received_any_frame;
    uint8_t last_seq_by_tool_id[LINK_WATCHDOG_TOOL_SLOTS];
    bool has_seq_by_tool_id[LINK_WATCHDOG_TOOL_SLOTS];
    urtc_rate_limiter_t command_rate_by_tool_id[LINK_WATCHDOG_TOOL_SLOTS];
} link_watchdog_t;

// A `timeout_ms` of 0 is replaced by LINK_WATCHDOG_DEFAULT_TIMEOUT_MS.
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

// Latches a link loss: once `timeout_ms` has elapsed since the last real
// frame, the link is marked as never proven alive again and every tracked
// sequence is forgotten. Call it from the main loop at least once per timeout.
// Without it a rack that heard nothing for more than 2^32 ms (about 49.7 days)
// would see its millisecond difference wrap around and the link would look
// alive again until the timeout passed a second time.
void link_watchdog_tick(link_watchdog_t *lw, uint32_t now_ms);

// Real idempotency/anti-reorder check for one continuous session: a
// sequence number that is not genuinely newer than the last one accepted
// for this exact tool_id (a real duplicate resend, or a stale/reordered
// older frame - see this project's own comment in link_watchdog.c) is rejected,
// not just an exact repeat of the immediately-previous value. Correctly
// handles real 8-bit wraparound (255 -> 0). Only a real, present tool_id
// (see tool_id.h) is ever tracked - an absent/invalid slot always returns
// false rather than silently allocating tracking state for it. This is
// NOT cryptographic anti-replay protection on its own - see
// link_watchdog_reset_all_sequences() for the real session-boundary
// handling a bare sequence counter cannot provide by itself.
bool link_watchdog_accept_sequence(link_watchdog_t *lw, uint8_t tool_id, uint8_t seq);

// The same decision as link_watchdog_accept_sequence() without recording anything: true when `seq` is genuinely newer than the last one accepted for the tool (or none was).
bool link_watchdog_is_sequence_new(const link_watchdog_t *lw, uint8_t tool_id, uint8_t seq);

// clears every per-tool "last sequence seen" slot - call this
// whenever a real, independent signal (link_watchdog_is_link_lost()
// having been true just before this frame) means the previous session's
// sequence state can no longer be trusted as a comparison baseline for a
// fresh one. rack_link_process_frame() already does this automatically;
// exposed here for any other real caller with its own session-boundary
// signal (e.g. a future protocol revision with an explicit boot/session
// counter).
void link_watchdog_reset_all_sequences(link_watchdog_t *lw);

// Sets the least time (ms) between two accepted commands for one tool, for every tool; 0 switches the limit off. The time already counted is forgotten.
void link_watchdog_set_command_interval(link_watchdog_t *lw, uint32_t min_interval_ms);

// The interval in force (0 = no limit); 0 too for a missing watchdog.
uint32_t link_watchdog_command_interval(const link_watchdog_t *lw);

// True when a command for `tool_id` may be acted on now: none was accepted for it before, or at least the interval has passed since the last one (across the wrap of the
// millisecond counter). It records the time when it answers true. A tool that is not tracked, or a missing watchdog, is refused. Only a command that would be applied
// should ask: a refusal of the rack (a value outside the limits) is never held back by it.
bool link_watchdog_allow_command(link_watchdog_t *lw, uint8_t tool_id, uint32_t now_ms);

#endif // LINK_WATCHDOG_H
