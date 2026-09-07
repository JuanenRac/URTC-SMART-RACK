// =============================================================================
// URTC-SMART-RACK Firmware - tests for tool_id.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/tool_id.h"

void run_tool_id_tests(int *failures)
{
    TEST_ASSERT(tool_id_decode(0x00) == 0, "raw 0x00 decodes to ID 0");
    TEST_ASSERT(tool_id_decode(0x1F) == 0x1F, "raw 0x1F decodes to ID 31 (all bits set)");
    TEST_ASSERT(tool_id_decode(0xFF) == 0x1F, "extra high bits above the 5-bit field are masked off");
    TEST_ASSERT(tool_id_decode(0x20) == 0x00, "bit 5 alone (outside the field) decodes to 0");
    TEST_ASSERT(tool_id_decode(0x15) == 0x15, "a mid-range ID passes through unchanged");

    TEST_ASSERT(tool_id_is_present(TOOL_ID_NONE) == false, "TOOL_ID_NONE is not a present tool");
    TEST_ASSERT(tool_id_is_present(0) == true, "ID 0 is a present tool");
    TEST_ASSERT(tool_id_is_present(30) == true, "ID 30 is a present tool");
}
