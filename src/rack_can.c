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

rack_command_status_t rack_can_decode_set_preheat(uint32_t can_id, const uint8_t *data, uint8_t dlc, uint8_t *out_seq, rack_set_preheat_t *out_command)
{
    if (data == NULL || out_seq == NULL || out_command == NULL || can_id != RACK_CAN_ID_SET_PREHEAT || dlc != RACK_CAN_DLC) {
        return RACK_CMD_ERR_WRONG_PAYLOAD_LEN;
    }
    // The validation of the command is written for the serial payload (tool, temperature
    // little-endian): hand it the same three bytes so there is one set of rules.
    const uint8_t serial_payload[3] = {data[0], data[2], data[1]};
    const rack_command_status_t status = rack_command_validate_set_preheat(serial_payload, 3u, out_command);
    if (status == RACK_CMD_OK) {
        *out_seq = data[3];
    }
    return status;
}
