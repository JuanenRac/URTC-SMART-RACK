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
    // a link the watchdog already considers lost (never proven
    // alive, or `timeout_ms` really elapsed since the last real frame)
    // means every previously tracked per-tool sequence number is stale
    // evidence from a session that is over - the tool board on the other
    // end may itself have rebooted and restarted its own counter low.
    // Reset before evaluating THIS frame so its sequence is never
    // mistaken for a stale replay of an old session's value.
    if (link_watchdog_is_link_lost(lw, now_ms)) {
        link_watchdog_reset_all_sequences(lw);
    }
    // any frame that reaches here already has a real, CRC-valid
    // framing (protocol_parse_frame() proved that) - that alone is honest
    // evidence the link itself is up, independent of which command it
    // carries or whether that command's own sequence gets re-applied.
    // Marking it here, instead of only after a successful
    // link_watchdog_accept_sequence() below, also fixes a real reliability
    // gap: a genuine resent duplicate (rejected on purpose, see Scenario 5
    // in tests/test_rack_link_scenarios.c) used to NOT count as link
    // activity, which could let a tool board that is only ever resending
    // its last command (e.g. because it never saw an ack) get spuriously
    // declared "lost" by link_watchdog_is_link_lost() despite real frames
    // still arriving.
    link_watchdog_note_frame_received(lw, now_ms);

    if (frame.cmd != RACK_CMD_SET_PREHEAT) {
        return preheat_safe_state_temp_c(); // unrelated/unsupported command - no tool_id layout to read a sequence against
    }
    // RACK_CMD_SET_PREHEAT's own tool_id lives in payload[0] (see
    // rack_command.h). this used to be read unconditionally, before
    // ever checking that the payload the wire framing itself claims to
    // carry (frame.len) actually has that byte. protocol_parse_frame()
    // only ever copies `len` real bytes into frame.payload - the rest of
    // that fixed-size array is left exactly as it was in this function's
    // own uninitialized local `frame`, so a real, validly-framed
    // zero-payload frame (len == 0 - protocol.h documents this as the
    // smallest legal frame) meant tool_id_decode() read undefined stack
    // garbage, whose decoded "tool" then had its anti-replay sequence slot
    // silently consumed/corrupted by link_watchdog_accept_sequence() below
    // using this unrelated frame's own seq byte.
    if (frame.len < 1u) {
        return preheat_safe_state_temp_c(); // no tool_id byte actually present - never guess one from uninitialized memory
    }
    if (!link_watchdog_accept_sequence(lw, tool_id_decode(frame.payload[0]), frame.seq)) {
        return preheat_safe_state_temp_c(); // a real duplicate/stale/reordered resend is not re-applied
    }

    rack_set_preheat_t command;
    if (rack_command_validate_set_preheat(frame.payload, frame.len, &command) != RACK_CMD_OK) {
        return preheat_safe_state_temp_c(); // out-of-range/malformed command -> safe state, not a guess
    }
    return command.target_temp_c;
}
