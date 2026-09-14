// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_inventory.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/rack_inventory.h"
#include "../src/tool_id.h"

void run_rack_inventory_tests(int *failures)
{
    rack_inventory_t inv;
    rack_inventory_init(&inv);

    TEST_ASSERT(rack_inventory_occupied_count(&inv) == 0, "a freshly-initialized rack has no occupied slots");

    rack_slot_t slot0;
    TEST_ASSERT(rack_inventory_get_slot(&inv, 0, &slot0), "slot 0 is a valid read on init");
    TEST_ASSERT(slot0.occupied == false, "slot 0 starts empty");
    TEST_ASSERT(tool_id_is_present(slot0.tool_id) == false, "an empty slot's tool_id is never a present tool");

    // Real multi-slot behavior: two different tools racked in two
    // different slots at once, independently tracked.
    TEST_ASSERT(rack_inventory_set_slot_tool(&inv, 0, 5), "racking tool 5 in slot 0 succeeds");
    TEST_ASSERT(rack_inventory_set_slot_tool(&inv, 1, 9), "racking tool 9 in slot 1 succeeds");
    TEST_ASSERT(rack_inventory_occupied_count(&inv) == 2, "two slots are now occupied");

    rack_slot_t s0, s1;
    rack_inventory_get_slot(&inv, 0, &s0);
    rack_inventory_get_slot(&inv, 1, &s1);
    TEST_ASSERT(s0.occupied && s0.tool_id == 5, "slot 0 really holds tool 5");
    TEST_ASSERT(s1.occupied && s1.tool_id == 9, "slot 1 really holds tool 9, independent of slot 0");

    uint8_t found_slot = 0xFF;
    TEST_ASSERT(rack_inventory_find_tool(&inv, 9, &found_slot), "tool 9 is found somewhere in the rack");
    TEST_ASSERT(found_slot == 1, "tool 9 is correctly reported as being in slot 1");
    TEST_ASSERT(rack_inventory_find_tool(&inv, 42, &found_slot) == false, "a tool that was never racked is not found");

    // The real physical invariant this module exists to enforce: a real
    // tool can never be in two slots at once - moving tool 5 into slot 2
    // must clear it out of slot 0.
    TEST_ASSERT(rack_inventory_set_slot_tool(&inv, 2, 5), "re-racking tool 5 into slot 2 succeeds");
    rack_slot_t new_slot0, new_slot2;
    rack_inventory_get_slot(&inv, 0, &new_slot0);
    rack_inventory_get_slot(&inv, 2, &new_slot2);
    TEST_ASSERT(new_slot0.occupied == false, "the same tool can never be reported in its old slot after moving");
    TEST_ASSERT(new_slot2.occupied && new_slot2.tool_id == 5, "the moved tool is now correctly in its new slot");
    TEST_ASSERT(rack_inventory_occupied_count(&inv) == 2, "a real move never changes the total occupied count");

    // Pre-heat targets are per-slot state, reached by tool_id (the caller
    // only ever knows "which tool", not "which slot it happens to sit in
    // right now").
    TEST_ASSERT(rack_inventory_set_preheat_target(&inv, 9, 220) == true, "setting a pre-heat target for a racked tool succeeds");
    rack_inventory_get_slot(&inv, 1, &s1);
    TEST_ASSERT(s1.target_temp_c == 220, "the pre-heat target really lands on the slot holding that tool");
    TEST_ASSERT(rack_inventory_set_preheat_target(&inv, 123, 200) == false, "a pre-heat target for a tool that isn't racked anywhere is refused, not silently accepted");

    // Re-racking a tool must not carry a stale pre-heat target forward.
    TEST_ASSERT(rack_inventory_set_slot_tool(&inv, 3, 9), "moving tool 9 (which had a pre-heat target) to slot 3 succeeds");
    rack_slot_t s3;
    rack_inventory_get_slot(&inv, 3, &s3);
    TEST_ASSERT(s3.target_temp_c == 0, "a freshly re-racked tool starts with no carried-over pre-heat target");

    // Clearing a slot: the TOOL_ID_NONE wire convention this project
    // already uses everywhere else for "no tool".
    TEST_ASSERT(rack_inventory_set_slot_tool(&inv, 3, TOOL_ID_NONE), "clearing a slot with TOOL_ID_NONE succeeds");
    rack_inventory_get_slot(&inv, 3, &s3);
    TEST_ASSERT(s3.occupied == false, "a cleared slot is no longer occupied");
    TEST_ASSERT(rack_inventory_find_tool(&inv, 9, &found_slot) == false, "a cleared tool is no longer found anywhere");

    // Fail-closed on an out-of-range slot - never reads/writes past the array.
    rack_slot_t out_of_range;
    TEST_ASSERT(rack_inventory_get_slot(&inv, RACK_INVENTORY_MAX_SLOTS, &out_of_range) == false, "reading an out-of-range slot is refused");
    TEST_ASSERT(rack_inventory_set_slot_tool(&inv, RACK_INVENTORY_MAX_SLOTS, 1) == false, "writing an out-of-range slot is refused");
    TEST_ASSERT(rack_inventory_get_slot(&inv, 255, &out_of_range) == false, "a far out-of-range slot is refused, not just the first one past the end");
}
