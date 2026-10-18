// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_inventory.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/rack_inventory.h"
#include "../src/tool_id.h"
#include "../src/tool_limits.h"

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
    TEST_ASSERT(rack_inventory_set_slot_tool(&inv, 1, 10), "racking tool 10 in slot 1 succeeds");
    TEST_ASSERT(rack_inventory_occupied_count(&inv) == 2, "two slots are now occupied");

    rack_slot_t s0, s1;
    rack_inventory_get_slot(&inv, 0, &s0);
    rack_inventory_get_slot(&inv, 1, &s1);
    TEST_ASSERT(s0.occupied && s0.tool_id == 5, "slot 0 really holds tool 5");
    TEST_ASSERT(s1.occupied && s1.tool_id == 10, "slot 1 really holds tool 10, independent of slot 0");

    uint8_t found_slot = 0xFF;
    TEST_ASSERT(rack_inventory_find_tool(&inv, 10, &found_slot), "tool 10 is found somewhere in the rack");
    TEST_ASSERT(found_slot == 1, "tool 10 is correctly reported as being in slot 1");
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
    TEST_ASSERT(rack_inventory_set_preheat_target(&inv, 10, 220) == true, "setting a pre-heat target for a racked tool succeeds");
    rack_inventory_get_slot(&inv, 1, &s1);
    TEST_ASSERT(s1.target_temp_c == 220, "the pre-heat target really lands on the slot holding that tool");
    TEST_ASSERT(rack_inventory_set_preheat_target(&inv, 123, 200) == false, "a pre-heat target for a tool that isn't racked anywhere is refused, not silently accepted");

    // Re-racking a tool must not carry a stale pre-heat target forward.
    TEST_ASSERT(rack_inventory_set_slot_tool(&inv, 3, 10), "moving tool 10 (which had a pre-heat target) to slot 3 succeeds");
    rack_slot_t s3;
    rack_inventory_get_slot(&inv, 3, &s3);
    TEST_ASSERT(s3.target_temp_c == 0, "a freshly re-racked tool starts with no carried-over pre-heat target");

    // Clearing a slot: the TOOL_ID_NONE wire convention this project
    // already uses everywhere else for "no tool".
    TEST_ASSERT(rack_inventory_set_slot_tool(&inv, 3, TOOL_ID_NONE), "clearing a slot with TOOL_ID_NONE succeeds");
    rack_inventory_get_slot(&inv, 3, &s3);
    TEST_ASSERT(s3.occupied == false, "a cleared slot is no longer occupied");
    TEST_ASSERT(rack_inventory_find_tool(&inv, 10, &found_slot) == false, "a cleared tool is no longer found anywhere");

    // Fail-closed on an out-of-range slot - never reads/writes past the array.
    rack_slot_t out_of_range;
    TEST_ASSERT(rack_inventory_get_slot(&inv, RACK_INVENTORY_MAX_SLOTS, &out_of_range) == false, "reading an out-of-range slot is refused");
    TEST_ASSERT(rack_inventory_set_slot_tool(&inv, RACK_INVENTORY_MAX_SLOTS, 1) == false, "writing an out-of-range slot is refused");
    TEST_ASSERT(rack_inventory_get_slot(&inv, 255, &out_of_range) == false, "a far out-of-range slot is refused, not just the first one past the end");

    // Reconciling against the physical slot-presence sensors after a power
    // cut: slot 0 still holds tool 5 (sensor agrees), tool 10 in slot 1 was
    // pulled while the rack was off (sensor empty), and someone dropped an
    // unknown tool into slot 3.
    rack_inventory_t r;
    rack_inventory_init(&r);
    rack_inventory_set_slot_tool(&r, 0, 5);
    rack_inventory_set_slot_tool(&r, 1, 10);
    rack_inventory_set_preheat_target(&r, 10, 300);
    rack_reconcile_report_t rep = rack_inventory_reconcile(&r, (uint8_t)((1u << 0) | (1u << 3)));
    TEST_ASSERT(rep.removed_mask == (1u << 1), "reconcile reports exactly the slot whose tool was pulled");
    TEST_ASSERT(rep.unidentified_mask == (1u << 3), "reconcile reports the slot with an unknown tool");
    rack_slot_t rs;
    rack_inventory_get_slot(&r, 1, &rs);
    TEST_ASSERT(!rs.occupied && rs.target_temp_c == 0, "a pulled tool's slot is cleared, pre-heat target included");
    TEST_ASSERT(rack_inventory_set_preheat_target(&r, 10, 300) == false, "the pulled tool can no longer be pre-heated");
    rack_inventory_get_slot(&r, 3, &rs);
    TEST_ASSERT(!rs.occupied, "an unidentified tool stays unoccupied until it is identified");
    rack_inventory_get_slot(&r, 0, &rs);
    TEST_ASSERT(rs.occupied && rs.tool_id == 5, "a slot that agrees with its sensor is untouched");
    rep = rack_inventory_reconcile(&r, (uint8_t)((1u << 0) | (1u << 3)));
    TEST_ASSERT(rep.removed_mask == 0 && rep.unidentified_mask == (1u << 3), "reconcile is idempotent");

    // A missing inventory is refused everywhere instead of being dereferenced.
    {
        rack_slot_t any_slot;
        uint8_t any_index;
        rack_inventory_init(NULL);
        TEST_ASSERT(!rack_inventory_set_slot_tool(NULL, 0, 1), "racking into a missing inventory is refused");
        TEST_ASSERT(!rack_inventory_get_slot(NULL, 0, &any_slot), "reading a missing inventory is refused");
        TEST_ASSERT(!rack_inventory_get_slot(&inv, 0, NULL), "reading into a missing output is refused");
        TEST_ASSERT(!rack_inventory_find_tool(NULL, 1, &any_index), "searching a missing inventory finds nothing");
        TEST_ASSERT(!rack_inventory_find_tool(&inv, 1, NULL), "searching into a missing output is refused");
        TEST_ASSERT(!rack_inventory_set_preheat_target(NULL, 0, 100), "a target in a missing inventory is refused");
        TEST_ASSERT(rack_inventory_occupied_count(NULL) == 0, "a missing inventory holds nothing");
        rack_reconcile_report_t none = rack_inventory_reconcile(NULL, 0xFF);
        TEST_ASSERT(none.removed_mask == 0 && none.unidentified_mask == 0, "reconciling a missing inventory reports nothing");
    }

    // Only a tool profile of the URTC catalogue (0..24) can be racked.
    {
        rack_inventory_t t;
        rack_slot_t ts;
        rack_inventory_init(&t);
        TEST_ASSERT(rack_inventory_set_slot_tool(&t, 0, 0), "tool 0 (the lowest profile) can be racked");
        TEST_ASSERT(rack_inventory_set_slot_tool(&t, 1, TOOL_ID_URTC_LAST), "tool 24 (the highest profile) can be racked");
        TEST_ASSERT(!rack_inventory_set_slot_tool(&t, 2, 25), "tool 25 names no tool profile and is refused");
        TEST_ASSERT(!rack_inventory_set_slot_tool(&t, 2, 30), "tool 30 names no tool profile and is refused");
        TEST_ASSERT(!rack_inventory_set_slot_tool(&t, 2, 0x3F), "a value with bits outside the 5-bit field is refused, not stored");
        rack_inventory_get_slot(&t, 2, &ts);
        TEST_ASSERT(!ts.occupied, "a refused write leaves the slot empty");
    }

    // The inventory applies the per-tool limit itself.
    {
        rack_inventory_t t;
        rack_slot_t ts;
        rack_inventory_init(&t);
        rack_inventory_set_slot_tool(&t, 0, TOOL_ID_3D_PRINTER);
        rack_inventory_set_slot_tool(&t, 1, 5); // a drill: no heater
        TEST_ASSERT(rack_inventory_set_preheat_target(&t, TOOL_ID_3D_PRINTER, 310), "the hotend accepts exactly its limit");
        TEST_ASSERT(!rack_inventory_set_preheat_target(&t, TOOL_ID_3D_PRINTER, 311), "the hotend refuses one degree above its limit");
        rack_inventory_get_slot(&t, 0, &ts);
        TEST_ASSERT(ts.target_temp_c == 310, "a refused target leaves the stored one untouched");
        TEST_ASSERT(!rack_inventory_set_preheat_target(&t, 5, 100), "a tool without a heater accepts no target above 0");
        TEST_ASSERT(rack_inventory_set_preheat_target(&t, 5, 0), "but 0 (off) is always accepted for a racked tool");
    }
}
