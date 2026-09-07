// =============================================================================
// URTC-SMART-RACK Firmware - Real receive-path decision: rack_link.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "rack_link.h"

#include "preheat.h"
#include "protocol.h"
#include "rack_command.h"
#include "tool_id.h"

uint16_t rack_link_process_frame(link_watchdog_t *lw, const uint8_t *buf, uint8_t buf_len, uint32_t now_ms)
{
    protocol_frame_t frame;
    if (protocol_parse_frame(buf, buf_len, &frame) != PROTOCOL_OK) {
        return preheat_safe_state_temp_c(); // a corrupt/malformed frame never reaches command validation
    }
    // RACK-01: a link the watchdog already considers lost (never proven
    // alive, or `timeout_ms` really elapsed since the last real frame)
    // means every previously tracked per-tool sequence number is stale
    // evidence from a session that is over - the tool board on the other
    // end may itself have rebooted and restarted its own counter low.
    // Reset before evaluating THIS frame so its sequence is never
    // mistaken for a stale replay of an old session's value.
    if (link_watchdog_is_link_lost(lw, now_ms)) {
        link_watchdog_reset_all_sequences(lw);
    }
    if (!link_watchdog_accept_sequence(lw, tool_id_decode(frame.payload[0]), frame.seq)) {
        return preheat_safe_state_temp_c(); // a real duplicate/stale/reordered resend is not re-applied
    }
    link_watchdog_note_frame_received(lw, now_ms);

    if (frame.cmd != RACK_CMD_SET_PREHEAT) {
        return preheat_safe_state_temp_c();
    }
    rack_set_preheat_t command;
    if (rack_command_validate_set_preheat(frame.payload, frame.len, &command) != RACK_CMD_OK) {
        return preheat_safe_state_temp_c(); // out-of-range/malformed command -> safe state, not a guess
    }
    return command.target_temp_c;
}
