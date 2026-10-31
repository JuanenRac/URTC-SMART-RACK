// =============================================================================
// URTC-SMART-RACK Firmware - CAN mapping of the rack command: rack_can.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "rack_can.h"

#include <stddef.h>

uint8_t rack_can_encode_set_preheat(uint8_t seq, uint8_t tool_id, uint16_t target_temp_c, uint8_t out_data[8])
{
    if (out_data == NULL) {
        return 0;
    }
    out_data[0] = tool_id;
    out_data[1] = (uint8_t)(target_temp_c >> 8);
    out_data[2] = (uint8_t)(target_temp_c & 0xFFu);
    out_data[3] = seq;
    out_data[4] = 0;
    out_data[5] = 0;
    out_data[6] = 0;
    out_data[7] = 0;
    return RACK_CAN_DLC;
}

static bool reply_code_exists(uint8_t code)
{
    return code <= RACK_REPLY_UNKNOWN_COMMAND;
}

uint8_t rack_can_encode_reply(const rack_decision_t *decision, uint8_t out_data[RACK_CAN_REPLY_DLC])
{
    if (decision == NULL || out_data == NULL || !reply_code_exists(decision->reply)) {
        return 0u;
    }
    out_data[0] = decision->reply_cmd;
    out_data[1] = decision->reply_tool;
    out_data[2] = decision->reply;
    return RACK_CAN_REPLY_DLC;
}

bool rack_can_parse_reply(uint32_t can_id, const uint8_t *data, uint8_t dlc, rack_can_reply_t *out)
{
    if (data == NULL || out == NULL || can_id != RACK_CAN_ID_REPLY || dlc != RACK_CAN_REPLY_DLC || !reply_code_exists(data[2])) {
        return false;
    }
    out->ack = data[2] == RACK_REPLY_APPLIED || data[2] == RACK_REPLY_REPEAT;
    out->cmd = data[0];
    out->tool_id = data[1];
    out->code = data[2];
    return true;
}

rack_command_status_t rack_can_decode_set_preheat(uint32_t can_id, const uint8_t *data, uint8_t dlc, uint8_t *out_seq, rack_set_preheat_t *out_command)
{
    if (data == NULL || out_seq == NULL || out_command == NULL || can_id != RACK_CAN_ID_SET_PREHEAT || dlc != RACK_CAN_DLC) {
        return RACK_CMD_ERR_WRONG_PAYLOAD_LEN;
    }
    // The serial payload and the first three data bytes of the CAN form are the same bytes (tool,
    // temperature big-endian): hand them to the same validation so there is one set of rules.
    const rack_command_status_t status = rack_command_validate_set_preheat(data, 3u, out_command);
    if (status == RACK_CMD_OK) {
        *out_seq = data[3];
    }
    return status;
}
