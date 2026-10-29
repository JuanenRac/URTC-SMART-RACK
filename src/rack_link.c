// =============================================================================
// URTC-SMART-RACK Firmware - Real receive-path decision: rack_link.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "rack_link.h"

#include <stdbool.h>
#include <stddef.h>

#include "preheat.h"
#include "protocol.h"
#include "rack_can.h"
#include "rack_command.h"
#include "tool_id.h"

// A frame that reached here has real, valid framing: evidence the link itself is up. A link the watchdog already considers lost (never proven alive, or `timeout_ms` really
// elapsed since the last real frame) means every previously tracked per-tool sequence number is stale evidence from a session that is over - the tool board on the other end
// may itself have rebooted and restarted its own counter low. Reset before evaluating THIS frame so its sequence is never mistaken for a stale replay of an old session's value.
static void note_valid_frame(link_watchdog_t *lw, uint32_t now_ms)
{
    link_watchdog_tick(lw, now_ms);
    if (link_watchdog_is_link_lost(lw, now_ms)) {
        link_watchdog_reset_all_sequences(lw);
    }
    // Any frame that reaches here is honest evidence the link is up, whichever command it carries and whether or not its sequence is re-applied (a retransmission of a command
    // that was applied is proof the transport works, not a sign of silence).
    link_watchdog_note_frame_received(lw, now_ms);
}

// What the sender is told when the validation refused the command.
static uint8_t reply_for(rack_command_status_t status)
{
    switch (status) {
        case RACK_CMD_ERR_WRONG_PAYLOAD_LEN: return RACK_REPLY_WRONG_LENGTH;
        case RACK_CMD_ERR_TOOL_ID_ABSENT: return RACK_REPLY_TOOL_ABSENT;
        case RACK_CMD_ERR_TEMP_OUT_OF_RANGE: return RACK_REPLY_TEMP_OUT_OF_RANGE;
        case RACK_CMD_ERR_TOOL_ID_INVALID: return RACK_REPLY_TOOL_INVALID;
        case RACK_CMD_ERR_TOOL_NOT_HEATABLE: return RACK_REPLY_NOT_HEATABLE;
        case RACK_CMD_OK: return RACK_REPLY_APPLIED;
        default: return RACK_REPLY_NONE;
    }
}
// The decision for a SET_PREHEAT whose tool byte is `tool`, whose validation gave `status` (and `command` when it was OK), carried by a frame with sequence `seq`.
static void decide_set_preheat(link_watchdog_t *lw, uint8_t seq, uint8_t tool, rack_command_status_t status, const rack_set_preheat_t *command, uint32_t now_ms, rack_decision_t *out)
{
    // The tool byte of a command is a plain 5-bit value naming a tool profile (see tool_id.h). A byte that is not, or names no tool, leaves nobody to act on: the frame changes
    // nothing (it is not evidence that any heater should be switched off).
    out->reply_cmd = RACK_CMD_SET_PREHEAT;
    out->reply_seq = seq;
    out->reply_tool = tool;
    const bool tool_readable = tool_id_is_wire_byte_valid(tool) && tool_id_is_present(tool) && tool_id_is_valid_urtc(tool);

    // The command is checked BEFORE its sequence number is spent: a frame with a valid CRC but a refused command (out of range, wrong length) must not advance the anti-replay
    // counter of the tool, or the sender's next good frame with that same sequence number would be thrown away as a duplicate.
    if (status != RACK_CMD_OK) {
        out->reply = reply_for(status);
        if (tool_readable) {
            out->action = RACK_ACT_SAFE_STATE; // out-of-range/malformed command -> that tool's safe state, not a guess
            out->tool_id = tool;
            out->temp_c = preheat_safe_state_temp_c();
        }
        return;
    }
    // A repeat of what was already applied (or a stale or reordered frame) is acknowledged without spending the interval: nothing to apply again, and the sender may stop
    // resending.
    if (!link_watchdog_is_sequence_new(lw, tool, seq)) {
        out->acknowledge = 1;
        out->reply = RACK_REPLY_REPEAT;
        return;
    }
    // A command that would be applied is held back when the same tool had one accepted less than the configured interval ago (a flood). It changes nothing and is not
    // acknowledged: the frame still proved the link alive (noted before), its sequence is not spent, and the sender's next frame after the interval is obeyed. The refusals
    // above are never throttled: a safe state is always applied.
    if (!link_watchdog_allow_command(lw, tool, now_ms)) {
        out->reply = RACK_REPLY_HELD_RATE;
        return;
    }
    if (!link_watchdog_accept_sequence(lw, tool, seq)) {
        return; // not reachable after the check above; if it ever were, a stale command is still never applied
    }
    out->action = RACK_ACT_SET_TARGET;
    out->tool_id = tool;
    out->temp_c = command->target_temp_c;
    out->acknowledge = 1;
    out->reply = RACK_REPLY_APPLIED;
}

static void clear_decision(rack_decision_t *out)
{
    out->action = RACK_ACT_NONE;
    out->tool_id = 0;
    out->temp_c = 0;
    out->acknowledge = 0;
    out->reply = RACK_REPLY_NONE;
    out->reply_cmd = 0;
    out->reply_seq = 0;
    out->reply_tool = 0xFFu;
}

void rack_link_decide(link_watchdog_t *lw, const uint8_t *buf, uint8_t buf_len, uint32_t now_ms, rack_decision_t *out)
{
    if (out == NULL) {
        return;
    }
    clear_decision(out);
    if (lw == NULL) {
        return;
    }

    protocol_frame_t frame;
    if (protocol_parse_frame(buf, buf_len, &frame) != PROTOCOL_OK) {
        return; // a corrupt/malformed frame never reaches command validation, and changes nothing: noise must not switch a heater off
    }
    note_valid_frame(lw, now_ms);

    out->reply_cmd = frame.cmd;
    out->reply_seq = frame.seq;
    if (frame.cmd != RACK_CMD_SET_PREHEAT) {
        out->reply = RACK_REPLY_UNKNOWN_COMMAND; // told so, but nothing changes: no tool_id layout to read a sequence against
        return;
    }
    // RACK_CMD_SET_PREHEAT's tool_id lives in payload[0]: it is never read from a frame that does not carry it (the rest of the fixed-size array is not the sender's).
    if (frame.len < 1u) {
        out->reply = RACK_REPLY_WRONG_LENGTH;
        return;
    }
    rack_set_preheat_t command = {0, 0};
    const rack_command_status_t status = rack_command_validate_set_preheat(frame.payload, frame.len, &command);
    decide_set_preheat(lw, frame.seq, frame.payload[0], status, &command, now_ms, out);
}

void rack_link_decide_can(link_watchdog_t *lw, uint32_t can_id, const uint8_t *data, uint8_t dlc, uint32_t now_ms, rack_decision_t *out)
{
    if (out == NULL) {
        return;
    }
    clear_decision(out);
    if (lw == NULL || data == NULL || can_id != RACK_CAN_ID_SET_PREHEAT || dlc != RACK_CAN_DLC) {
        return; // not a rack command frame: the CAN hardware already checked its CRC, the identifier and the length decide whether it is ours
    }
    note_valid_frame(lw, now_ms);
    rack_set_preheat_t command = {0, 0};
    uint8_t seq = 0;
    const rack_command_status_t status = rack_can_decode_set_preheat(can_id, data, dlc, &seq, &command);
    if (status != RACK_CMD_OK) {
        seq = data[3];
    }
    decide_set_preheat(lw, seq, data[0], status, &command, now_ms, out);
}

bool rack_link_rx_byte(rack_deframer_t *rx, link_watchdog_t *lw, uint8_t byte, uint32_t now_ms, rack_decision_t *out)
{
    uint8_t frame[PROTOCOL_MAX_FRAME_SIZE];
    const uint8_t length = rack_deframer_push(rx, byte, now_ms, frame);
    if (length == 0u) {
        return false;
    }
    rack_link_decide(lw, frame, length, now_ms, out);
    return true;
}

bool rack_link_rx_next(rack_deframer_t *rx, link_watchdog_t *lw, uint32_t now_ms, rack_decision_t *out)
{
    uint8_t frame[PROTOCOL_MAX_FRAME_SIZE];
    const uint8_t length = rack_deframer_next(rx, frame);
    if (length == 0u) {
        return false;
    }
    rack_link_decide(lw, frame, length, now_ms, out);
    return true;
}
