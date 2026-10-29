// =============================================================================
// URTC-SMART-RACK Firmware - tests for preheat_limit.c and its use in rack_app.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// A pre-heat that no swap ever follows must end by itself even though the host link is alive.
#include <stddef.h>

#include "test_runner.h"
#include "../src/preheat_limit.h"
#include "../src/rack_app.h"
#include "../src/rack_command.h"
#include "../src/tool_limits.h"

#define MINUTE 60000u

static uint16_t target_of(const rack_inventory_t *inv, uint8_t tool)
{
    uint8_t slot;
    rack_slot_t s;
    if (!rack_inventory_find_tool(inv, tool, &slot) || !rack_inventory_get_slot(inv, slot, &s)) {
        return 0xFFFF;
    }
    return s.target_temp_c;
}

static rack_decision_t set_target(uint8_t tool, uint16_t temp)
{
    rack_decision_t d = {RACK_ACT_SET_TARGET, tool, temp, 1, RACK_REPLY_APPLIED, RACK_CMD_SET_PREHEAT, tool, 0};
    return d;
}

void run_preheat_limit_tests(int *failures)
{
    preheat_limit_t pl;

    // Configuration: 0 means the default of ten minutes, and the limit can be turned off.
    preheat_limit_init(&pl, 0);
    TEST_ASSERT(pl.max_ms == RACK_PREHEAT_MAX_MS && RACK_PREHEAT_MAX_MS == 10u * MINUTE, "the default limit is ten minutes");
    preheat_limit_init(&pl, 3u * MINUTE);
    TEST_ASSERT(pl.max_ms == 3u * MINUTE, "the limit is configurable");
    preheat_limit_init(NULL, 1);

    rack_inventory_t inv;
    link_watchdog_t lw;
    rack_inventory_init(&inv);
    link_watchdog_init(&lw, 5000);
    rack_inventory_set_slot_tool(&inv, 0, TOOL_ID_SOLDERING_IRON);
    rack_inventory_set_slot_tool(&inv, 1, TOOL_ID_3D_PRINTER);
    preheat_limit_init(&pl, 10u * MINUTE);

    // The pre-heat of the iron starts at t=1000; the host keeps resending the same target while the link stays alive.
    rack_decision_t iron200 = set_target(TOOL_ID_SOLDERING_IRON, 200);
    TEST_ASSERT(rack_app_apply_decision_limited(&inv, &pl, &iron200, 1000), "the pre-heat starts");
    for (uint32_t t = 1000; t < 1000u + 9u * MINUTE; t += 2000u) {
        link_watchdog_note_frame_received(&lw, t);
        (void)rack_app_apply_decision_limited(&inv, &pl, &iron200, t); // the host repeats itself
        (void)rack_app_tick_limited(&inv, &lw, &pl, t);
    }
    TEST_ASSERT(target_of(&inv, TOOL_ID_SOLDERING_IRON) == 200, "nine minutes in, the pre-heat is still on");

    // Resending does not restart the clock: at ten minutes it ends although the link is alive.
    const uint32_t end = 1000u + 10u * MINUTE;
    link_watchdog_note_frame_received(&lw, end);
    TEST_ASSERT(!rack_app_tick_limited(&inv, &lw, &pl, end), "the link is alive");
    TEST_ASSERT(target_of(&inv, TOOL_ID_SOLDERING_IRON) == 0, "ten minutes without a swap switch the heater off on a living link");
    TEST_ASSERT(preheat_limit_is_expired(&pl, 0), "the slot is latched");

    // The host that keeps sending 200 cannot start it again.
    TEST_ASSERT(!rack_app_apply_decision_limited(&inv, &pl, &iron200, end + 1) && target_of(&inv, TOOL_ID_SOLDERING_IRON) == 0, "a repeated target is refused while the slot is latched");
    (void)rack_app_tick_limited(&inv, &lw, &pl, end + 2);
    TEST_ASSERT(target_of(&inv, TOOL_ID_SOLDERING_IRON) == 0, "and the heater stays off");

    // An explicit 0 from the host acknowledges it; after that a new pre-heat starts a new clock.
    rack_decision_t iron0 = set_target(TOOL_ID_SOLDERING_IRON, 0);
    TEST_ASSERT(rack_app_apply_decision_limited(&inv, &pl, &iron0, end + 3) && !preheat_limit_is_expired(&pl, 0), "a target of 0 clears the latch");
    TEST_ASSERT(rack_app_apply_decision_limited(&inv, &pl, &iron200, end + 4) && target_of(&inv, TOOL_ID_SOLDERING_IRON) == 200, "a new pre-heat is accepted");
    link_watchdog_note_frame_received(&lw, end + 5);
    (void)rack_app_tick_limited(&inv, &lw, &pl, end + 5);
    TEST_ASSERT(target_of(&inv, TOOL_ID_SOLDERING_IRON) == 200, "and it has its own full ten minutes");

    // Changing the target of a running pre-heat does not restart the clock either.
    preheat_limit_init(&pl, 10u * MINUTE);
    rack_inventory_set_preheat_target(&inv, TOOL_ID_3D_PRINTER, 0);
    rack_decision_t printer = set_target(TOOL_ID_3D_PRINTER, 200);
    (void)rack_app_apply_decision_limited(&inv, &pl, &printer, 0);
    printer.temp_c = 250;
    (void)rack_app_apply_decision_limited(&inv, &pl, &printer, 9u * MINUTE);
    link_watchdog_note_frame_received(&lw, 10u * MINUTE);
    (void)rack_app_tick_limited(&inv, &lw, &pl, 10u * MINUTE);
    TEST_ASSERT(target_of(&inv, TOOL_ID_3D_PRINTER) == 0, "raising the target at minute nine does not buy another ten minutes");

    // A swap (the tool in the slot is replaced) clears the latch.
    TEST_ASSERT(preheat_limit_is_expired(&pl, 1), "the printer slot is latched");
    rack_inventory_set_slot_tool(&inv, 1, TOOL_ID_HOTAIR_REWORK);
    link_watchdog_note_frame_received(&lw, 10u * MINUTE + 1);
    (void)rack_app_tick_limited(&inv, &lw, &pl, 10u * MINUTE + 1);
    TEST_ASSERT(!preheat_limit_is_expired(&pl, 1), "another tool in the slot ends the latch");
    rack_decision_t hotair = set_target(TOOL_ID_HOTAIR_REWORK, 300);
    TEST_ASSERT(rack_app_apply_decision_limited(&inv, &pl, &hotair, 10u * MINUTE + 2) && target_of(&inv, TOOL_ID_HOTAIR_REWORK) == 300, "the new tool can be pre-heated");

    // The clock of one slot does not depend on another slot.
    preheat_limit_init(&pl, 10u * MINUTE);
    rack_inventory_set_preheat_target(&inv, TOOL_ID_SOLDERING_IRON, 0);
    rack_inventory_set_preheat_target(&inv, TOOL_ID_HOTAIR_REWORK, 0);
    (void)rack_app_apply_decision_limited(&inv, &pl, &iron200, 0);
    (void)rack_app_apply_decision_limited(&inv, &pl, &hotair, 5u * MINUTE);
    link_watchdog_note_frame_received(&lw, 10u * MINUTE);
    (void)rack_app_tick_limited(&inv, &lw, &pl, 10u * MINUTE);
    TEST_ASSERT(target_of(&inv, TOOL_ID_SOLDERING_IRON) == 0 && target_of(&inv, TOOL_ID_HOTAIR_REWORK) == 300, "the iron ran out, the hot-air head started later has not");

    // A lost link ends the pre-heat without a latch: when the host is back it can command again.
    preheat_limit_init(&pl, 10u * MINUTE);
    rack_inventory_set_preheat_target(&inv, TOOL_ID_SOLDERING_IRON, 0);
    link_watchdog_init(&lw, 1000);
    (void)rack_app_apply_decision_limited(&inv, &pl, &iron200, 100);
    link_watchdog_note_frame_received(&lw, 100);
    TEST_ASSERT(rack_app_tick_limited(&inv, &lw, &pl, 5000) && target_of(&inv, TOOL_ID_SOLDERING_IRON) == 0, "a lost link brings the iron to the safe state");
    TEST_ASSERT(!preheat_limit_is_expired(&pl, 0), "a lost link is not a latch");
    link_watchdog_note_frame_received(&lw, 6000);
    TEST_ASSERT(rack_app_apply_decision_limited(&inv, &pl, &iron200, 6000) && target_of(&inv, TOOL_ID_SOLDERING_IRON) == 200, "the host can pre-heat again once the link is back");

    // The limit can be turned off from the configuration: then only the link watchdog and the per-tool ceilings bound the heater.
    preheat_limit_init(&pl, PREHEAT_LIMIT_OFF);
    rack_inventory_set_preheat_target(&inv, TOOL_ID_SOLDERING_IRON, 0);
    link_watchdog_init(&lw, 1000);
    (void)rack_app_apply_decision_limited(&inv, &pl, &iron200, 0);
    for (uint32_t t = 0; t <= 60u * MINUTE; t += 500u) {
        link_watchdog_note_frame_received(&lw, t);
        (void)rack_app_tick_limited(&inv, &lw, &pl, t);
    }
    TEST_ASSERT(target_of(&inv, TOOL_ID_SOLDERING_IRON) == 200 && !preheat_limit_is_expired(&pl, 0), "with the limit off an hour of resent targets stays on");

    // Wrap of the millisecond counter in the middle of a pre-heat.
    preheat_limit_init(&pl, 10u * MINUTE);
    rack_inventory_set_preheat_target(&inv, TOOL_ID_SOLDERING_IRON, 0);
    link_watchdog_init(&lw, 5000);
    const uint32_t before_wrap = 0xFFFFFFFFu - 5u * MINUTE;
    (void)rack_app_apply_decision_limited(&inv, &pl, &iron200, before_wrap);
    link_watchdog_note_frame_received(&lw, before_wrap + 5u * MINUTE + 1u);
    (void)rack_app_tick_limited(&inv, &lw, &pl, before_wrap + 5u * MINUTE + 1u);
    TEST_ASSERT(target_of(&inv, TOOL_ID_SOLDERING_IRON) == 200, "five minutes in, across the wrap, still on");
    link_watchdog_note_frame_received(&lw, before_wrap + 10u * MINUTE);
    (void)rack_app_tick_limited(&inv, &lw, &pl, before_wrap + 10u * MINUTE);
    TEST_ASSERT(target_of(&inv, TOOL_ID_SOLDERING_IRON) == 0, "ten minutes across the wrap of the clock end the pre-heat");

    // A target for a tool that is not racked starts no clock; arguments are checked.
    preheat_limit_init(&pl, 10u * MINUTE);
    rack_decision_t ghost = set_target(TOOL_ID_HOTAIR_REWORK, 300);
    rack_inventory_t empty;
    rack_inventory_init(&empty);
    TEST_ASSERT(!rack_app_apply_decision_limited(&empty, &pl, &ghost, 0) && !pl.running[0], "a tool that is not racked is refused and starts no clock");
    TEST_ASSERT(!preheat_limit_allow(NULL, 0, 0, 100, 0) && !preheat_limit_allow(&pl, RACK_INVENTORY_MAX_SLOTS, 0, 100, 0), "missing limit or slot out of range is refused");
    TEST_ASSERT(preheat_limit_tick(NULL, &inv, false, 0) == 0 && preheat_limit_tick(&pl, NULL, false, 0) == 0, "tick refuses missing arguments");
    TEST_ASSERT(!preheat_limit_is_expired(NULL, 0) && !preheat_limit_is_expired(&pl, 200), "expired refuses missing arguments");
    TEST_ASSERT(!rack_app_apply_decision_limited(NULL, &pl, &ghost, 0) && !rack_app_apply_decision_limited(&inv, &pl, NULL, 0), "the limited apply refuses missing arguments");
}
