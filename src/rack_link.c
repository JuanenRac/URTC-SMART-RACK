// =============================================================================
// URTC-SMART-RACK Firmware - Real receive-path decision: rack_link.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "rack_link.h"

#include <stddef.h>

#include "preheat.h"
#include "protocol.h"
#include "rack_command.h"
#include "tool_id.h"

void rack_link_decide(link_watchdog_t *lw, const uint8_t *buf, uint8_t buf_len, uint32_t now_ms, rack_decision_t *out)
{
    if (out == NULL) {
        return;
    }
    out->action = RACK_ACT_NONE;
    out->tool_id = 0;
    out->temp_c = 0;
    out->acknowledge = 0;
    if (lw == NULL) {
        return;
    }

    protocol_frame_t frame;
    if (protocol_parse_frame(buf, buf_len, &frame) != PROTOCOL_OK) {
        return; // a corrupt/malformed frame never reaches command validation, and changes nothing: noise must not switch a heater off
    }
    // A link the watchdog already considers lost (never proven alive, or `timeout_ms` really elapsed since the last real frame) means every previously tracked per-tool sequence
    // number is stale evidence from a session that is over - the tool board on the other end may itself have rebooted and restarted its own counter low. Reset before evaluating
    // THIS frame so its sequence is never mistaken for a stale replay of an old session's value.
    if (link_watchdog_is_link_lost(lw, now_ms)) {
        link_watchdog_reset_all_sequences(lw);
    }
    // Any frame that reaches here has a real, CRC-valid framing: honest evidence the link itself is up, whichever command it carries and whether or not its sequence is re-applied
    // (a retransmission of a command that was applied is proof the transport works, not a sign of silence).
    link_watchdog_note_frame_received(lw, now_ms);

    if (frame.cmd != RACK_CMD_SET_PREHEAT) {
        return; // a command this rack does not act on: no tool_id layout to read a sequence against, and nothing to change
    }
    // RACK_CMD_SET_PREHEAT's tool_id lives in payload[0]: it is never read from a frame that does not carry it (the rest of the fixed-size array is not the sender's).
    if (frame.len < 1u) {
        return;
    }
    const uint8_t tool = tool_id_decode(frame.payload[0]);

    // The command is checked BEFORE its sequence number is spent: a frame with a valid CRC but a refused command (out of range, wrong length) must not advance the anti-replay
    // counter of the tool, or the sender's next good frame with that same sequence number would be thrown away as a duplicate.
    rack_set_preheat_t command;
    if (rack_command_validate_set_preheat(frame.payload, frame.len, &command) != RACK_CMD_OK) {
        if (tool_id_is_present(tool)) {
            out->action = RACK_ACT_SAFE_STATE; // out-of-range/malformed command -> that tool's safe state, not a guess
            out->tool_id = tool;
            out->temp_c = preheat_safe_state_temp_c();
        }
        return;
    }
    if (!link_watchdog_accept_sequence(lw, tool, frame.seq)) {
        out->acknowledge = 1; // a retransmission of what was already applied (or a stale/reordered frame): nothing to apply again, and the sender may stop resending
        return;
    }
    out->action = RACK_ACT_SET_TARGET;
    out->tool_id = tool;
    out->temp_c = command.target_temp_c;
    out->acknowledge = 1;
}
