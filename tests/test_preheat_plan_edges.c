// =============================================================================
// URTC-SMART-RACK Firmware - edge tests for preheat_plan.c (the numbers at their limits)
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The plan turns the job queue into orders. These checks hold its arithmetic at the ends of every range: the largest margin and target, a hold
// time so long that adding it to the lead time would wrap round, a full inventory, and jobs listed in any order.
#include <stdint.h>

#include "../src/preheat_plan.h"
#include "../src/rack_inventory.h"
#include "../src/tool_id.h"
#include "../src/tool_limits.h"
#include "test_runner.h"

void run_preheat_plan_edge_tests(int *failures)
{
    preheat_order_t orders[PREHEAT_PLAN_MAX_ORDERS];
    rack_inventory_t inv;
    rack_inventory_init(&inv);
    rack_inventory_set_slot_tool(&inv, 0, TOOL_ID_SOLDERING_IRON);

    // the lead time at the ends of its inputs
    TEST_ASSERT(preheat_plan_lead_ms(TOOL_ID_SOLDERING_IRON, 25u, 50u) == 0u, "a target at room temperature needs no climb");
    TEST_ASSERT(preheat_plan_lead_ms(TOOL_ID_SOLDERING_IRON, 26u, 0u) == 100u, "one degree above room temperature at 10 s per 100 degrees is 100 ms");
    TEST_ASSERT(preheat_plan_lead_ms(TOOL_ID_SOLDERING_IRON, 200u, 0u) == 17500u, "175 degrees at 10 s per 100 degrees with no margin");
    TEST_ASSERT(preheat_plan_lead_ms(TOOL_ID_SOLDERING_IRON, 200u, 100u) == 35000u, "a margin of 100 percent doubles the time");
    TEST_ASSERT(preheat_plan_lead_ms(TOOL_ID_SOLDERING_IRON, 200u, 255u) == 17500u + 17500u * 255u / 100u, "the largest margin does not wrap");
    TEST_ASSERT(preheat_plan_lead_ms(TOOL_ID_SOLDERING_IRON, 65535u, 255u) > 0u, "the largest target gives a finite time");
    TEST_ASSERT(preheat_plan_lead_ms(TOOL_ID_SOLDERING_IRON, 65535u, 255u) > preheat_plan_lead_ms(TOOL_ID_SOLDERING_IRON, 65534u, 255u), "and it still grows with the target");
    TEST_ASSERT(preheat_plan_lead_ms(0u, 200u, 50u) == 0u && preheat_plan_lead_ms(255u, 200u, 50u) == 0u, "a tool without a known climb rate has no lead time");

    // a job exactly at, and either side of, the lead time
    preheat_plan_config_t cfg = preheat_plan_config_default();
    cfg.enabled = true;
    const uint32_t lead = preheat_plan_lead_ms(TOOL_ID_SOLDERING_IRON, preheat_plan_target_c(&cfg, TOOL_ID_SOLDERING_IRON), cfg.margin_percent);
    const preheat_job_t at[] = {{TOOL_ID_SOLDERING_IRON, lead}};
    const preheat_job_t after[] = {{TOOL_ID_SOLDERING_IRON, lead + 1u}};
    const preheat_job_t before[] = {{TOOL_ID_SOLDERING_IRON, lead - 1u}};
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, at, 1u, orders) == 1u, "a job exactly at the lead time orders the pre-heat");
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, before, 1u, orders) == 1u, "one millisecond inside it too");
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, after, 1u, orders) == 0u, "one millisecond outside it does not");

    // the margin changes the answer: the same job is ordered with a big margin and not with none
    const preheat_job_t mid[] = {{TOOL_ID_SOLDERING_IRON, 20000u}};
    cfg.margin_percent = 0u;
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, mid, 1u, orders) == 0u, "17.5 s of climb: a job in 20 s waits when there is no margin");
    cfg.margin_percent = 50u;
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, mid, 1u, orders) == 1u, "and is ordered with a margin of 50 percent (26.25 s)");

    // a hold time so long that adding it to the lead time would wrap round must keep a hot tool warm, not switch it off
    rack_inventory_set_preheat_target(&inv, TOOL_ID_SOLDERING_IRON, 200u);
    cfg.cool_unused = true;
    cfg.hold_ms = 0xFFFFFFFFu;
    const preheat_job_t far_job[] = {{TOOL_ID_SOLDERING_IRON, 3600000u}};
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, far_job, 1u, orders) == 0u, "an endless hold keeps the hot tool warm for a job an hour away");
    cfg.hold_ms = 0u;
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, far_job, 1u, orders) == 1u && orders[0].target_c == 0u, "no hold: the tool is switched off at once");

    // a hot tool with no job at all is switched off, and one that is cold is left alone
    cfg.hold_ms = 120000u;
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, NULL, 0u, orders) == 1u && orders[0].target_c == 0u, "no job: the hot tool is switched off");
    rack_inventory_set_preheat_target(&inv, TOOL_ID_SOLDERING_IRON, 0u);
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, NULL, 0u, orders) == 0u, "a tool that is already off gets no order");

    // the earliest job of a tool decides, in whatever order the jobs are listed
    cfg.cool_unused = false;
    const preheat_job_t first_near[] = {{TOOL_ID_SOLDERING_IRON, 1000u}, {TOOL_ID_SOLDERING_IRON, 9000000u}};
    const preheat_job_t last_near[] = {{TOOL_ID_SOLDERING_IRON, 9000000u}, {TOOL_ID_SOLDERING_IRON, 1000u}};
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, first_near, 2u, orders) == 1u, "the near job listed first");
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, last_near, 2u, orders) == 1u, "the near job listed last");

    // a job for a tool that is not in the rack gives no order, and a zero count ignores the list
    const preheat_job_t other[] = {{TOOL_ID_HOTAIR_REWORK, 0u}};
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, other, 1u, orders) == 0u, "a job for a tool that is not racked");
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, first_near, 0u, orders) == 0u, "a count of zero ignores the list");

    // a full rack of the same two heaters never gives more orders than there are slots
    rack_inventory_init(&inv);
    for (uint8_t slot = 0; slot < RACK_INVENTORY_MAX_SLOTS; slot++) {
        rack_inventory_set_slot_tool(&inv, slot, (slot % 2u) == 0u ? TOOL_ID_SOLDERING_IRON : TOOL_ID_HOTAIR_REWORK);
    }
    const preheat_job_t both[] = {{TOOL_ID_SOLDERING_IRON, 0u}, {TOOL_ID_HOTAIR_REWORK, 0u}};
    const uint8_t n = preheat_plan_decide(&cfg, &inv, both, 2u, orders);
    TEST_ASSERT(n <= PREHEAT_PLAN_MAX_ORDERS, "never more orders than slots");
    TEST_ASSERT(n >= 1u, "a full rack with due jobs orders at least one pre-heat");
    for (uint8_t i = 0; i < n; i++) {
        TEST_ASSERT(orders[i].target_c > 0u && orders[i].target_c <= tool_limits_max_preheat_c(orders[i].tool_id), "every order is within the ceiling of its tool");
    }
}
