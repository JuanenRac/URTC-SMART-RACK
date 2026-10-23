// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_app.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include <stddef.h>

#include "test_runner.h"
#include "../src/protocol.h"
#include "../src/rack_app.h"
#include "../src/rack_command.h"
#include "../src/tool_limits.h"

static uint16_t target_of(const rack_inventory_t *inv, uint8_t tool)
{
    uint8_t slot;
    rack_slot_t s;
    if (!rack_inventory_find_tool(inv, tool, &slot) || !rack_inventory_get_slot(inv, slot, &s)) {
        return 0xFFFF;
    }
    return s.target_temp_c;
}

static void deliver(rack_inventory_t *inv, link_watchdog_t *lw, uint8_t seq, uint8_t tool, uint16_t temp, uint32_t now_ms)
{
    uint8_t payload[3] = {tool, (uint8_t)(temp >> 8), (uint8_t)(temp & 0xFFu)};
    uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
    uint8_t len = protocol_encode_frame(seq, RACK_CMD_SET_PREHEAT, payload, 3, buf);
    rack_decision_t d;
    rack_link_decide(lw, buf, len, now_ms, &d);
    rack_app_apply_decision(inv, &d);
}

void run_rack_app_tests(int *failures)
{
    rack_inventory_t inv;
    link_watchdog_t lw;
    rack_inventory_init(&inv);
    link_watchdog_init(&lw, 1000);
    rack_inventory_set_slot_tool(&inv, 0, TOOL_ID_SOLDERING_IRON);
    rack_inventory_set_slot_tool(&inv, 1, TOOL_ID_3D_PRINTER);

    // A frame for each tool lands on the slot that holds that tool.
    deliver(&inv, &lw, 1, TOOL_ID_SOLDERING_IRON, 200, 100);
    deliver(&inv, &lw, 1, TOOL_ID_3D_PRINTER, 250, 110);
    TEST_ASSERT(target_of(&inv, TOOL_ID_SOLDERING_IRON) == 200, "the soldering iron slot gets its own target");
    TEST_ASSERT(target_of(&inv, TOOL_ID_3D_PRINTER) == 250, "the hotend slot gets its own target");

    // A retransmission leaves both targets alone, and 'do nothing' writes nothing.
    deliver(&inv, &lw, 1, TOOL_ID_SOLDERING_IRON, 200, 120);
    TEST_ASSERT(target_of(&inv, TOOL_ID_SOLDERING_IRON) == 200 && target_of(&inv, TOOL_ID_3D_PRINTER) == 250, "a retransmission changes no target");
    rack_decision_t none = {RACK_ACT_NONE, 0, 0, 0};
    TEST_ASSERT(!rack_app_apply_decision(&inv, &none), "a decision to do nothing writes nothing");

    // A refused command puts only its tool in the safe state.
    deliver(&inv, &lw, 2, TOOL_ID_3D_PRINTER, 390, 130);
    TEST_ASSERT(target_of(&inv, TOOL_ID_3D_PRINTER) == 0 && target_of(&inv, TOOL_ID_SOLDERING_IRON) == 200, "a hotend request above its limit switches the hotend off and leaves the iron heated");

    // A decision for a tool that is not racked writes nothing.
    rack_decision_t ghost = {RACK_ACT_SET_TARGET, TOOL_ID_HOTAIR_REWORK, 300, 1};
    TEST_ASSERT(!rack_app_apply_decision(&inv, &ghost), "a target for a tool that is not in the rack is not stored");
    TEST_ASSERT(!rack_app_apply_decision(NULL, &ghost) && !rack_app_apply_decision(&inv, NULL), "missing arguments are refused");

    // While the link is alive the tick leaves the targets; when it is lost every slot goes to the safe state.
    TEST_ASSERT(!rack_app_tick(&inv, &lw, 500) && target_of(&inv, TOOL_ID_SOLDERING_IRON) == 200, "a living link keeps its targets");
    TEST_ASSERT(rack_app_tick(&inv, &lw, 1200), "the tick reports the lost link");
    TEST_ASSERT(target_of(&inv, TOOL_ID_SOLDERING_IRON) == 0 && target_of(&inv, TOOL_ID_3D_PRINTER) == 0, "a lost link brings every slot to the safe state");

    // A link that was never proven alive starts in the safe state.
    rack_inventory_t fresh;
    link_watchdog_t silent;
    rack_inventory_init(&fresh);
    link_watchdog_init(&silent, 1000);
    rack_inventory_set_slot_tool(&fresh, 0, TOOL_ID_SOLDERING_IRON);
    rack_inventory_set_preheat_target(&fresh, TOOL_ID_SOLDERING_IRON, 200);
    TEST_ASSERT(rack_app_tick(&fresh, &silent, 0) && target_of(&fresh, TOOL_ID_SOLDERING_IRON) == 0, "a rack that never heard the host holds no target");
    TEST_ASSERT(rack_app_enter_safe_state(&fresh) == 0, "entering the safe state again changes nothing");
    TEST_ASSERT(rack_app_enter_safe_state(NULL) == 0, "a missing inventory is refused");
}
