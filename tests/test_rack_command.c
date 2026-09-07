// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_command.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/rack_command.h"
#include "../src/tool_id.h"

void run_rack_command_tests(int *failures)
{
    // --- Real valid command ---
    {
        uint8_t payload[3] = {5, 0xC8, 0x00}; // tool_id=5, target_temp_c=200 (0x00C8, little-endian)
        rack_set_preheat_t out;
        rack_command_status_t status = rack_command_validate_set_preheat(payload, 3, &out);
        TEST_ASSERT(status == RACK_CMD_OK, "a real, in-range SET_PREHEAT command validates as RACK_CMD_OK");
        TEST_ASSERT(out.tool_id == 5, "decoded tool_id matches");
        TEST_ASSERT(out.target_temp_c == 200, "decoded target_temp_c matches (little-endian decode)");
    }

    // --- Real boundary: exactly RACK_COMMAND_MAX_TEMP_C is still valid ---
    {
        uint8_t payload[3] = {0, (uint8_t)(RACK_COMMAND_MAX_TEMP_C & 0xFF), (uint8_t)(RACK_COMMAND_MAX_TEMP_C >> 8)};
        rack_set_preheat_t out;
        TEST_ASSERT(rack_command_validate_set_preheat(payload, 3, &out) == RACK_CMD_OK, "a target temp exactly at RACK_COMMAND_MAX_TEMP_C is still accepted");
    }

    // --- Real out-of-range temperature is rejected, not clamped ---
    {
        uint16_t too_hot = RACK_COMMAND_MAX_TEMP_C + 1u;
        uint8_t payload[3] = {0, (uint8_t)(too_hot & 0xFF), (uint8_t)(too_hot >> 8)};
        rack_set_preheat_t out;
        rack_command_status_t status = rack_command_validate_set_preheat(payload, 3, &out);
        TEST_ASSERT(status == RACK_CMD_ERR_TEMP_OUT_OF_RANGE, "a target temp one degree above RACK_COMMAND_MAX_TEMP_C is rejected as RACK_CMD_ERR_TEMP_OUT_OF_RANGE");
    }

    // --- Real absent tool ID (TOOL_ID_NONE) is rejected ---
    {
        uint8_t payload[3] = {TOOL_ID_NONE, 0xC8, 0x00};
        rack_set_preheat_t out;
        rack_command_status_t status = rack_command_validate_set_preheat(payload, 3, &out);
        TEST_ASSERT(status == RACK_CMD_ERR_TOOL_ID_ABSENT, "a command targeting TOOL_ID_NONE (no tool present) is rejected as RACK_CMD_ERR_TOOL_ID_ABSENT");
    }

    // --- Real wrong payload length ---
    {
        uint8_t short_payload[2] = {0, 0};
        rack_set_preheat_t out;
        TEST_ASSERT(rack_command_validate_set_preheat(short_payload, 2, &out) == RACK_CMD_ERR_WRONG_PAYLOAD_LEN, "a 2-byte payload (SET_PREHEAT needs 3) is rejected as RACK_CMD_ERR_WRONG_PAYLOAD_LEN");

        uint8_t long_payload[4] = {0, 0, 0, 0};
        TEST_ASSERT(rack_command_validate_set_preheat(long_payload, 4, &out) == RACK_CMD_ERR_WRONG_PAYLOAD_LEN, "a 4-byte payload is also rejected as RACK_CMD_ERR_WRONG_PAYLOAD_LEN");
    }
}
