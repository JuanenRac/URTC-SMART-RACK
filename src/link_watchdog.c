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
    if (lw->has_seq_by_tool_id[tool_id] && lw->last_seq_by_tool_id[tool_id] == seq) {
        return false; // real duplicate for this tool slot
    }
    lw->last_seq_by_tool_id[tool_id] = seq;
    lw->has_seq_by_tool_id[tool_id] = true;
    return true;
}
