// =============================================================================
// URTC-SMART-RACK Firmware - Rack command validation: rack_command.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "rack_command.h"
#include "tool_id.h"
#include "tool_limits.h"
#include <stddef.h>

_Static_assert(RACK_COMMAND_MAX_TEMP_C == TOOL_LIMITS_RACK_CEILING_C, "the command ceiling and the per-tool ceiling must be the same value");

rack_command_status_t rack_command_validate_set_preheat(const uint8_t *payload, uint8_t len, rack_set_preheat_t *out_command)
{
    if (payload == NULL || out_command == NULL) {
        return RACK_CMD_ERR_WRONG_PAYLOAD_LEN;
    }
    if (len != 3u) {
        return RACK_CMD_ERR_WRONG_PAYLOAD_LEN;
    }

    // A tool byte on the wire is a plain 5-bit value: upper bits set mean
    // corruption or another protocol version, not "the same tool".
    if (!tool_id_is_wire_byte_valid(payload[0])) {
        return RACK_CMD_ERR_TOOL_ID_INVALID;
    }
    uint8_t tool_id = payload[0];
    if (!tool_id_is_present(tool_id)) {
        return RACK_CMD_ERR_TOOL_ID_ABSENT;
    }
    if (!tool_id_is_valid_urtc(tool_id)) {
        return RACK_CMD_ERR_TOOL_ID_INVALID;
    }

    // Little-endian, matching protocol.h's own wire convention.
    uint16_t target_temp_c = (uint16_t)(payload[1] | ((uint16_t)payload[2] << 8));
    if (target_temp_c > RACK_COMMAND_MAX_TEMP_C) {
        return RACK_CMD_ERR_TEMP_OUT_OF_RANGE;
    }
    const uint16_t tool_max_c = tool_limits_max_preheat_c(tool_id);
    if (target_temp_c > 0u && tool_max_c == 0u) {
        return RACK_CMD_ERR_TOOL_NOT_HEATABLE;
    }
    if (target_temp_c > tool_max_c) {
        return RACK_CMD_ERR_TEMP_OUT_OF_RANGE;
    }

    out_command->tool_id = tool_id;
    out_command->target_temp_c = target_temp_c;
    return RACK_CMD_OK;
}
