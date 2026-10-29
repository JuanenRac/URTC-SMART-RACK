// =============================================================================
// URTC-SMART-RACK Firmware - tests for tool_id.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/tool_id.h"
#include "../src/tool_limits.h"

// The numbering is the one generated once for every repository that uses it (urtc_tool_ids.h); a rack that drifts from it does not compile.
_Static_assert((1u << TOOL_ID_BITS) - 1u == URTC_TOOL_ID_MASK, "five bits carry the tool identifier");
_Static_assert(TOOL_ID_URTC_LAST == URTC_TOOL_ID_COUNT - 1u, "the last profile is the last identifier of the catalogue");
_Static_assert(TOOL_ID_FREE_CONFIG == 31u && TOOL_ID_NONE == URTC_TOOL_ID_FREE_CONFIG, "31 is the free configuration of a head");
_Static_assert(URTC_TOOL_ID_FIRST_INVALID == TOOL_ID_URTC_LAST + 1u, "the identifiers after the last profile are not assigned");
_Static_assert(TOOL_ID_SOLDERING_IRON == URTC_TOOL_ID_SOLDERING_IRON && TOOL_ID_3D_PRINTER == URTC_TOOL_ID_PRINTER_3D && TOOL_ID_HOTAIR_REWORK == URTC_TOOL_ID_HOTAIR_REWORK, "the heated profiles keep their identifiers");

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

    // The ID space is the one of URTC's tool heads: 0..24 profiles, 25..30 unassigned, 31 free configuration.
    TEST_ASSERT(tool_id_is_valid_urtc(0), "ID 0 is a tool profile");
    TEST_ASSERT(tool_id_is_valid_urtc(24), "ID 24 (the last profile) is a tool profile");
    TEST_ASSERT(!tool_id_is_valid_urtc(25), "ID 25 names no tool profile");
    TEST_ASSERT(!tool_id_is_valid_urtc(30), "ID 30 names no tool profile");
    TEST_ASSERT(!tool_id_is_valid_urtc(31), "ID 31 (free configuration) is not a tool profile by itself");
    TEST_ASSERT(TOOL_ID_FREE_CONFIG == 31u && TOOL_ID_NONE == 31u, "31 is both the free-configuration value of URTC and this rack's empty-slot marker");
    TEST_ASSERT(tool_id_is_wire_byte_valid(0x00) && tool_id_is_wire_byte_valid(0x1F), "a tool byte from 0 to 0x1F is a plain 5-bit value");
    TEST_ASSERT(!tool_id_is_wire_byte_valid(0x20) && !tool_id_is_wire_byte_valid(0xE3) && !tool_id_is_wire_byte_valid(0xFF), "a tool byte with any upper bit set is not");
}
