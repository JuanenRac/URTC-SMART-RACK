// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_command.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/rack_command.h"
#include "../src/tool_id.h"
#include "../src/tool_limits.h"

void run_rack_command_tests(int *failures)
{
    // --- Real valid command ---
    {
        uint8_t payload[3] = {TOOL_ID_SOLDERING_IRON, 0xC8, 0x00}; // tool_id=0, target_temp_c=200 (0x00C8, little-endian)
        rack_set_preheat_t out;
        rack_command_status_t status = rack_command_validate_set_preheat(payload, 3, &out);
        TEST_ASSERT(status == RACK_CMD_OK, "a real, in-range SET_PREHEAT command validates as RACK_CMD_OK");
        TEST_ASSERT(out.tool_id == TOOL_ID_SOLDERING_IRON, "decoded tool_id matches");
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

    // --- The tool byte is a plain 5-bit value naming a tool profile ---
    {
        rack_set_preheat_t out;
        uint8_t high_bits[3] = {0x20 | TOOL_ID_SOLDERING_IRON, 0xC8, 0x00};
        TEST_ASSERT(rack_command_validate_set_preheat(high_bits, 3, &out) == RACK_CMD_ERR_TOOL_ID_INVALID, "a tool byte with an upper bit set is corruption, not tool 0");
        uint8_t id25[3] = {25, 0xC8, 0x00};
        uint8_t id30[3] = {30, 0xC8, 0x00};
        TEST_ASSERT(rack_command_validate_set_preheat(id25, 3, &out) == RACK_CMD_ERR_TOOL_ID_INVALID, "tool 25 names no tool profile");
        TEST_ASSERT(rack_command_validate_set_preheat(id30, 3, &out) == RACK_CMD_ERR_TOOL_ID_INVALID, "tool 30 names no tool profile");
        uint8_t id31[3] = {31, 0xC8, 0x00};
        TEST_ASSERT(rack_command_validate_set_preheat(id31, 3, &out) == RACK_CMD_ERR_TOOL_ID_ABSENT, "31 (free configuration / empty) is never a tool to heat");
        uint8_t id24[3] = {24, 0, 0};
        TEST_ASSERT(rack_command_validate_set_preheat(id24, 3, &out) == RACK_CMD_OK && out.tool_id == 24 && out.target_temp_c == 0, "the last profile is a valid tool, and 0 (off) is valid for it");
    }

    // --- Each tool has its own limit, and a tool without a heater has none ---
    {
        rack_set_preheat_t out;
        uint8_t hotend_ok[3] = {TOOL_ID_3D_PRINTER, 0x36, 0x01};   // 310
        uint8_t hotend_hot[3] = {TOOL_ID_3D_PRINTER, 0x37, 0x01};  // 311
        uint8_t hotend_390[3] = {TOOL_ID_3D_PRINTER, 0x86, 0x01};  // 390
        TEST_ASSERT(rack_command_validate_set_preheat(hotend_ok, 3, &out) == RACK_CMD_OK && out.target_temp_c == 310, "the 3D printer hotend accepts exactly 310 C");
        TEST_ASSERT(rack_command_validate_set_preheat(hotend_hot, 3, &out) == RACK_CMD_ERR_TEMP_OUT_OF_RANGE, "and refuses 311 C");
        TEST_ASSERT(rack_command_validate_set_preheat(hotend_390, 3, &out) == RACK_CMD_ERR_TEMP_OUT_OF_RANGE, "390 C is below the rack ceiling but far above what the hotend accepts");
        uint8_t gripper_hot[3] = {7, 0xC8, 0x00};
        uint8_t gripper_off[3] = {7, 0x00, 0x00};
        TEST_ASSERT(rack_command_validate_set_preheat(gripper_hot, 3, &out) == RACK_CMD_ERR_TOOL_NOT_HEATABLE, "a gripper has no heater: any target above 0 is refused");
        TEST_ASSERT(rack_command_validate_set_preheat(gripper_off, 3, &out) == RACK_CMD_OK && out.target_temp_c == 0, "but 'off' is a valid request for it");
        TEST_ASSERT(tool_limits_max_preheat_c(TOOL_ID_SOLDERING_IRON) == RACK_COMMAND_MAX_TEMP_C, "a soldering iron is held to the rack ceiling");
        TEST_ASSERT(tool_limits_max_preheat_c(TOOL_ID_HOTAIR_REWORK) == RACK_COMMAND_MAX_TEMP_C, "so is the hot-air head");
        TEST_ASSERT(tool_limits_max_preheat_c(25) == 0 && tool_limits_max_preheat_c(31) == 0 && tool_limits_max_preheat_c(0xFF) == 0, "an ID that is not a tool profile has no limit to offer");
    }
}
