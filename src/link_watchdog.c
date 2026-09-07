// =============================================================================
// URTC-SMART-RACK Firmware - Link timeout + command idempotency: link_watchdog.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "link_watchdog.h"
#include "tool_id.h"
#include <stddef.h>

void link_watchdog_init(link_watchdog_t *lw, uint32_t timeout_ms)
{
    if (lw == NULL) {
        return;
    }
    lw->last_rx_ms = 0;
    lw->timeout_ms = timeout_ms;
    lw->has_received_any_frame = false;
    for (uint8_t i = 0; i < LINK_WATCHDOG_TOOL_SLOTS; i++) {
        lw->last_seq_by_tool_id[i] = 0;
        lw->has_seq_by_tool_id[i] = false;
    }
}

void link_watchdog_note_frame_received(link_watchdog_t *lw, uint32_t now_ms)
{
    if (lw == NULL) {
        return;
    }
    lw->last_rx_ms = now_ms;
    lw->has_received_any_frame = true;
}

bool link_watchdog_is_link_lost(const link_watchdog_t *lw, uint32_t now_ms)
{
    if (lw == NULL) {
        return true; // fail safe: no real watchdog state at all counts as lost
    }
    if (!lw->has_received_any_frame) {
        return true;
    }
    // Unsigned subtraction wraps correctly even across a real uint32_t
    // millisecond-counter rollover - no special-casing needed.
    return (now_ms - lw->last_rx_ms) >= lw->timeout_ms;
}

bool link_watchdog_accept_sequence(link_watchdog_t *lw, uint8_t tool_id, uint8_t seq)
{
    if (lw == NULL || !tool_id_is_present(tool_id) || tool_id >= LINK_WATCHDOG_TOOL_SLOTS) {
        return false;
    }
    if (lw->has_seq_by_tool_id[tool_id]) {
        // RACK-01 (found in an ecosystem-wide software-improvements
        // audit, P1): the old check only ever rejected an EXACT repeat
        // of the immediately-previous sequence - 10, 11, 10 accepted the
        // second 10 outright (10 != 11), re-applying a real stale/
        // replayed command as if it were fresh. This treats the 8-bit
        // sequence space as a signed ring instead: delta in (0, 127]
        // means `seq` is genuinely ahead of the last-accepted value,
        // including a real wraparound (254 -> 0 is delta +2); delta in
        // [-128, 0] means `seq` is an exact duplicate or a real replay/
        // reorder of an already-seen-or-older value and is rejected.
        // This is still bounded, ordinary idempotency/anti-reorder
        // protection scoped to one continuous session, not
        // cryptographic anti-replay - see link_watchdog_reset_all_sequences()
        // for the real session-boundary/epoch handling a bare sequence
        // counter cannot provide by itself.
        int8_t delta = (int8_t)(uint8_t)(seq - lw->last_seq_by_tool_id[tool_id]);
        if (delta <= 0) {
            return false;
        }
    }
    lw->last_seq_by_tool_id[tool_id] = seq;
    lw->has_seq_by_tool_id[tool_id] = true;
    return true;
}

void link_watchdog_reset_all_sequences(link_watchdog_t *lw)
{
    if (lw == NULL) {
        return;
    }
    // RACK-01: the real epoch boundary this watchdog can actually detect
    // without a new protocol field - a link recovering from real loss
    // (see link_watchdog_is_link_lost()) means every previously tracked
    // per-tool sequence number is stale evidence from a session that is
    // over, not real state to compare a fresh command against (the tool
    // board on the other end may itself have rebooted and restarted its
    // own counter low). rack_link_process_frame() calls this whenever it
    // observes the link was already lost, so the very first command of a
    // new session is never mistaken for a stale replay of an old one.
    for (uint8_t i = 0; i < LINK_WATCHDOG_TOOL_SLOTS; i++) {
        lw->has_seq_by_tool_id[i] = false;
    }
}
