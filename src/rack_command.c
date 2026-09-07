// =============================================================================
// URTC-SMART-RACK Firmware - Rack command validation: rack_command.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "rack_command.h"
#include "tool_id.h"
#include <stddef.h>

rack_command_status_t rack_command_validate_set_preheat(const uint8_t *payload, uint8_t len, rack_set_preheat_t *out_command)
{
    if (payload == NULL || out_command == NULL) {
        return RACK_CMD_ERR_WRONG_PAYLOAD_LEN;
    }
    if (len != 3u) {
        return RACK_CMD_ERR_WRONG_PAYLOAD_LEN;
    }

    uint8_t tool_id = tool_id_decode(payload[0]);
    if (!tool_id_is_present(tool_id)) {
        return RACK_CMD_ERR_TOOL_ID_ABSENT;
    }

    // Little-endian, matching protocol.h's own wire convention.
    uint16_t target_temp_c = (uint16_t)(payload[1] | ((uint16_t)payload[2] << 8));
    if (target_temp_c > RACK_COMMAND_MAX_TEMP_C) {
        return RACK_CMD_ERR_TEMP_OUT_OF_RANGE;
    }

    out_command->tool_id = tool_id;
    out_command->target_temp_c = target_temp_c;
    return RACK_CMD_OK;
}
