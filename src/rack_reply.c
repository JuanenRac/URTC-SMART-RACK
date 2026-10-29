// =============================================================================
// URTC-SMART-RACK Firmware - Answers to the host: rack_reply.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "rack_reply.h"

#include <stddef.h>

static bool code_exists(uint8_t code)
{
    return code <= RACK_REPLY_UNKNOWN_COMMAND;
}

static bool code_is_ack(uint8_t code)
{
    return code == RACK_REPLY_APPLIED || code == RACK_REPLY_REPEAT;
}

uint8_t rack_reply_encode(const rack_decision_t *decision, uint8_t out_buf[PROTOCOL_MAX_FRAME_SIZE])
{
    if (decision == NULL || out_buf == NULL || !code_exists(decision->reply)) {
        return 0u;
    }
    const uint8_t payload[RACK_REPLY_PAYLOAD_LEN] = {decision->reply_cmd, decision->reply_tool, decision->reply};
    return protocol_encode_frame(decision->reply_seq, code_is_ack(decision->reply) ? RACK_REPLY_CMD_ACK : RACK_REPLY_CMD_NACK, payload, RACK_REPLY_PAYLOAD_LEN, out_buf);
}

bool rack_reply_parse(const protocol_frame_t *frame, rack_reply_t *out)
{
    if (frame == NULL || out == NULL || (frame->cmd != RACK_REPLY_CMD_ACK && frame->cmd != RACK_REPLY_CMD_NACK) || frame->len != RACK_REPLY_PAYLOAD_LEN) {
        return false;
    }
    const uint8_t code = frame->payload[2];
    // The command byte and the code must agree: an acknowledgement carries APPLIED or REPEAT and nothing else, and a refusal never does.
    if (!code_exists(code) || (frame->cmd == RACK_REPLY_CMD_ACK) != code_is_ack(code)) {
        return false;
    }
    out->ack = frame->cmd == RACK_REPLY_CMD_ACK;
    out->cmd = frame->payload[0];
    out->tool_id = frame->payload[1];
    out->code = code;
    out->seq = frame->seq;
    return true;
}