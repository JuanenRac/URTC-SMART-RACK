// =============================================================================
// URTC-SMART-RACK Firmware - Host-side tests: heads, wear, thermal check and predictive pre-heat (test_rack_units.c)
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include <math.h>

#include "../src/preheat.h"
#include "../src/preheat_plan.h"
#include "../src/rack_inventory.h"
#include "../src/tool_id.h"
#include "../src/rack_units.h"
#include "../src/slot_verify.h"
#include "../src/tool_limits.h"
#include "test_runner.h"

static void test_identity_in_the_inventory(int *failures)
{
    rack_inventory_t inv;
    rack_inventory_init(&inv);
    TEST_ASSERT(rack_inventory_set_slot_unit(&inv, 0, TOOL_ID_SOLDERING_IRON, 7u), "rack a head with a serial number");
    TEST_ASSERT(rack_inventory_set_slot_unit(&inv, 1, TOOL_ID_SOLDERING_IRON, 9u), "the same type with another serial is another head");
    TEST_ASSERT(inv.slots[0].occupied && inv.slots[1].occupied && inv.slots[0].serial == 7u && inv.slots[1].serial == 9u, "both stay racked");
    TEST_ASSERT(rack_inventory_set_slot_unit(&inv, 2, TOOL_ID_SOLDERING_IRON, 7u), "head 7 moves to slot 2");
    TEST_ASSERT(!inv.slots[0].occupied && inv.slots[0].serial == 0u && inv.slots[2].occupied && inv.slots[1].occupied, "its old slot is cleared, the other head stays");
    inv.slots[2].target_temp_c = 250;
    TEST_ASSERT(rack_inventory_set_slot_unit(&inv, 3, TOOL_ID_SOLDERING_IRON, 0u), "a head of unknown serial of a type already racked");
    TEST_ASSERT(!inv.slots[1].occupied && !inv.slots[2].occupied && inv.slots[3].occupied && inv.slots[3].serial == 0u, "without identity the type alone decides: the others go");
    TEST_ASSERT(inv.slots[3].target_temp_c == 0u, "a head just racked has no target");
    TEST_ASSERT(rack_inventory_set_slot_unit(&inv, 4, TOOL_ID_SOLDERING_IRON, 5u), "a known head next to an unknown one");
    TEST_ASSERT(!inv.slots[3].occupied && inv.slots[4].serial == 5u, "the unknown one cannot be told apart from it and is replaced");
    TEST_ASSERT(rack_inventory_set_slot_unit(&inv, 4, TOOL_ID_NONE, 3u) && !inv.slots[4].occupied && inv.slots[4].serial == 0u, "racking nothing clears the serial");
    TEST_ASSERT(!rack_inventory_set_slot_unit(&inv, 0, 25u, 1u), "a tool outside the profiles is refused");
    TEST_ASSERT(!rack_inventory_set_slot_unit(&inv, RACK_INVENTORY_MAX_SLOTS, 1u, 1u), "a slot out of range is refused");
    rack_inventory_set_slot_unit(&inv, 5, 3u, 12u);
    const rack_reconcile_report_t rep = rack_inventory_reconcile(&inv, 0u);
    TEST_ASSERT(rep.removed_mask != 0u && inv.slots[5].serial == 0u, "a slot found empty loses its serial too");
}

static void test_units_and_counters(int *failures)
{
    rack_units_t u;
    rack_units_init(&u);
    TEST_ASSERT(rack_units_count(&u) == 0u, "no unit at the start");
    TEST_ASSERT(!rack_units_record_use(&u, TOOL_ID_SOLDERING_IRON, 0u, 10u), "a head without a serial number has no record");
    TEST_ASSERT(!rack_units_record_use(&u, 25u, 3u, 10u), "a type that is no tool profile has no record");
    TEST_ASSERT(rack_units_record_use(&u, TOOL_ID_SOLDERING_IRON, 7u, 30u) && rack_units_record_use(&u, TOOL_ID_SOLDERING_IRON, 7u, 20u), "use is recorded per unit");
    TEST_ASSERT(rack_units_record_use(&u, TOOL_ID_SOLDERING_IRON, 9u, 5u), "another serial is another unit");
    TEST_ASSERT(rack_units_count(&u) == 2u, "two units");
    rack_unit_t *a = rack_units_find(&u, TOOL_ID_SOLDERING_IRON, 7u);
    rack_unit_t *b = rack_units_find(&u, TOOL_ID_SOLDERING_IRON, 9u);
    TEST_ASSERT(a != NULL && b != NULL && a->use.total_cycles == 2u && a->use.total_seconds == 50u && b->use.total_cycles == 1u, "cycles and seconds follow the unit");
    TEST_ASSERT(rack_units_find(&u, TOOL_ID_SOLDERING_IRON, 8u) == NULL && rack_units_find(&u, TOOL_ID_HOTAIR_REWORK, 7u) == NULL, "serial and type must both match");

    // the counters of the head: only growth is added
    TEST_ASSERT(rack_units_note_counters(&u, TOOL_ID_SOLDERING_IRON, 7u, 2u, 3600u, 1u, true), "first reading");
    TEST_ASSERT(a->heater_seconds == 3600u && a->faults == 2u && a->last_fault == 1u, "the history the head already had belongs to the unit");
    rack_units_note_counters(&u, TOOL_ID_SOLDERING_IRON, 7u, 2u, 3600u, 1u, true);
    TEST_ASSERT(a->heater_seconds == 3600u && a->faults == 2u, "reading twice does not count twice");
    rack_units_note_counters(&u, TOOL_ID_SOLDERING_IRON, 7u, 3u, 4000u, 2u, true);
    TEST_ASSERT(a->heater_seconds == 4000u && a->faults == 3u && a->last_fault == 2u, "growth is added");
    rack_units_note_counters(&u, TOOL_ID_SOLDERING_IRON, 7u, 0u, 100u, 0u, false);
    TEST_ASSERT(a->heater_seconds == 4000u && a->faults == 3u, "a counter that went down (erased F-RAM) adds nothing it cannot prove");
    rack_units_note_counters(&u, TOOL_ID_SOLDERING_IRON, 7u, 0u, 160u, 0u, false);
    TEST_ASSERT(a->heater_seconds == 4060u, "and counts again from the new value");
    TEST_ASSERT(!rack_units_note_counters(&u, TOOL_ID_SOLDERING_IRON, 0u, 1u, 1u, 1u, true), "no serial, no record");
    rack_units_note_counters(&u, TOOL_ID_SOLDERING_IRON, 9u, 0u, UINT32_MAX, 0u, true);
    rack_units_note_counters(&u, TOOL_ID_SOLDERING_IRON, 9u, 0u, 5u, 0u, true);
    rack_units_note_counters(&u, TOOL_ID_SOLDERING_IRON, 9u, 0u, UINT32_MAX, 0u, true);
    TEST_ASSERT(b->heater_seconds == UINT32_MAX, "the total saturates and never wraps");

    // maintenance
    rack_maintenance_limits_t lim = {0u, 0u, 0u};
    TEST_ASSERT(!rack_units_needs_maintenance(&u, TOOL_ID_SOLDERING_IRON, 7u, &lim), "no limit set, no maintenance");
    lim.max_cycles = 2u;
    TEST_ASSERT(rack_units_needs_maintenance(&u, TOOL_ID_SOLDERING_IRON, 7u, &lim), "cycle limit reached");
    lim.max_cycles = 0u; lim.max_heater_seconds = 4060u;
    TEST_ASSERT(rack_units_needs_maintenance(&u, TOOL_ID_SOLDERING_IRON, 7u, &lim), "heater time limit reached");
    lim.max_heater_seconds = 0u; lim.max_faults = 4u;
    TEST_ASSERT(!rack_units_needs_maintenance(&u, TOOL_ID_SOLDERING_IRON, 7u, &lim), "three faults are below four");
    lim.max_faults = 3u;
    TEST_ASSERT(rack_units_needs_maintenance(&u, TOOL_ID_SOLDERING_IRON, 7u, &lim), "fault limit reached");
    TEST_ASSERT(!rack_units_needs_maintenance(&u, TOOL_ID_SOLDERING_IRON, 77u, &lim) && !rack_units_needs_maintenance(&u, TOOL_ID_SOLDERING_IRON, 7u, NULL), "an unknown unit or no limits: false");

    // a full table refuses more units but never overwrites
    rack_units_init(&u);
    for (uint8_t s = 1u; s <= RACK_UNITS_MAX; s++) {
        TEST_ASSERT(rack_units_record_use(&u, 1u, s, 1u), "fill the table");
    }
    TEST_ASSERT(!rack_units_record_use(&u, 1u, 200u, 1u) && rack_units_count(&u) == RACK_UNITS_MAX, "a full table refuses a new unit");
    TEST_ASSERT(rack_units_record_use(&u, 1u, 3u, 1u), "and keeps serving the known ones");
}

static void test_slot_verify(int *failures)
{
    TEST_ASSERT(slot_verify(true, 300u, NAN, 24.0f, 0u) == SLOT_VERDICT_UNKNOWN, "no reading");
    TEST_ASSERT(slot_verify(false, 0u, 80.0f, 24.0f, 0u) == SLOT_VERDICT_EMPTY_HOT && slot_verdict_is_fault(SLOT_VERDICT_EMPTY_HOT), "a heater on in an empty slot is a fault");
    TEST_ASSERT(slot_verify(false, 0u, 60.0f, 24.0f, 0u) == SLOT_VERDICT_EMPTY_OK, "a little warm is not");
    TEST_ASSERT(slot_verify(true, 300u, 290.0f, 24.0f, 1000u) == SLOT_VERDICT_REACHED && !slot_verdict_is_fault(SLOT_VERDICT_REACHED), "reached");
    TEST_ASSERT(slot_verify(true, 300u, 100.0f, 24.0f, 1000u) == SLOT_VERDICT_HEATING, "still heating");
    TEST_ASSERT(slot_verify(true, 300u, 100.0f, 24.0f, SLOT_VERIFY_REACH_TIMEOUT_MS) == SLOT_VERDICT_NOT_REACHED && slot_verdict_is_fault(SLOT_VERDICT_NOT_REACHED), "too slow is a fault");
    TEST_ASSERT(slot_verify(true, 300u, 340.0f, 24.0f, 100000u) == SLOT_VERDICT_OVERSHOOT, "overshoot");
    TEST_ASSERT(slot_verify(true, 0u, 200.0f, 24.0f, 0u) == SLOT_VERDICT_IDLE_HOT && slot_verify(true, 0u, 30.0f, 24.0f, 0u) == SLOT_VERDICT_IDLE_OK, "told to stay cold");
    TEST_ASSERT(!slot_verdict_is_fault(SLOT_VERDICT_HEATING) && !slot_verdict_is_fault(SLOT_VERDICT_UNKNOWN), "heating and unknown are not faults");
}

static void test_preheat_plan(int *failures)
{
    rack_inventory_t inv;
    rack_inventory_init(&inv);
    rack_inventory_set_slot_tool(&inv, 0, TOOL_ID_SOLDERING_IRON);
    rack_inventory_set_slot_tool(&inv, 1, TOOL_ID_HOTAIR_REWORK);
    rack_inventory_set_slot_tool(&inv, 2, 3u);      // a screwdriver: no heater
    preheat_order_t orders[PREHEAT_PLAN_MAX_ORDERS];
    preheat_plan_config_t cfg = preheat_plan_config_default();
    TEST_ASSERT(!cfg.enabled, "off by default");
    const preheat_job_t jobs[] = { { TOOL_ID_SOLDERING_IRON, 1000u }, { TOOL_ID_HOTAIR_REWORK, 600000u }, { 3u, 10u } };
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, jobs, 3u, orders) == 0u, "disabled: no order at all");

    cfg.enabled = true;
    TEST_ASSERT(preheat_plan_default_target_c(TOOL_ID_SOLDERING_IRON) == 200u && preheat_plan_default_target_c(TOOL_ID_HOTAIR_REWORK) == 350u && preheat_plan_default_target_c(3u) == 0u, "default targets");
    const uint32_t lead = preheat_plan_lead_ms(TOOL_ID_SOLDERING_IRON, 200u, 50u);
    TEST_ASSERT(lead == 26250u, "175 degrees at 10 s per 100, plus 50 percent");
    TEST_ASSERT(preheat_plan_lead_ms(3u, 200u, 50u) == 0u && preheat_plan_lead_ms(TOOL_ID_SOLDERING_IRON, 20u, 50u) == 0u, "no heater or no climb: no lead time");
    uint8_t n = preheat_plan_decide(&cfg, &inv, jobs, 3u, orders);
    TEST_ASSERT(n == 1u && orders[0].tool_id == TOOL_ID_SOLDERING_IRON && orders[0].target_c == 200u, "only the iron is due: its job starts inside its lead time");

    const preheat_job_t later[] = { { TOOL_ID_SOLDERING_IRON, 26251u }, { TOOL_ID_HOTAIR_REWORK, 100000u } };
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, later, 2u, orders) == 0u, "one millisecond outside the lead time: nothing yet");
    const preheat_job_t edge[] = { { TOOL_ID_SOLDERING_IRON, 26250u } };
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, edge, 1u, orders) == 1u, "exactly the lead time: order");

    // two jobs of one tool: the earliest counts
    const preheat_job_t two[] = { { TOOL_ID_SOLDERING_IRON, 500000u }, { TOOL_ID_SOLDERING_IRON, 5000u } };
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, two, 2u, orders) == 1u, "the earliest job of a tool decides");

    // already at the target: no repeated order
    inv.slots[0].target_temp_c = 200u;
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, jobs, 3u, orders) == 0u, "target already set: no order");

    // a job already due
    const preheat_job_t due[] = { { TOOL_ID_HOTAIR_REWORK, 0u } };
    n = preheat_plan_decide(&cfg, &inv, due, 1u, orders);
    TEST_ASSERT(n == 1u && orders[0].tool_id == TOOL_ID_HOTAIR_REWORK && orders[0].target_c == 350u, "a job that is due now orders the pre-heat");

    // a tool that is not racked gives no order; cooling is optional
    rack_inventory_init(&inv);
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, jobs, 3u, orders) == 0u, "nothing racked, nothing to heat");
    rack_inventory_set_slot_tool(&inv, 0, TOOL_ID_SOLDERING_IRON);
    inv.slots[0].target_temp_c = 200u;
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, later, 0u, orders) == 0u, "a hot tool with no job is left alone by default");
    cfg.cool_unused = true;
    n = preheat_plan_decide(&cfg, &inv, later, 0u, orders);
    TEST_ASSERT(n == 1u && orders[0].target_c == 0u, "with cool_unused it is switched off");
    const preheat_job_t soon[] = { { TOOL_ID_SOLDERING_IRON, 60000u } };
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, soon, 1u, orders) == 0u, "but kept warm when its job is within lead plus hold");
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, NULL, 1u, orders) == 0u && preheat_plan_decide(NULL, &inv, soon, 1u, orders) == 0u, "bad arguments: no order");

    // a 3D printer hotend: no default target, the person sets one, and the ceiling of the tool holds it
    rack_inventory_init(&inv);
    rack_inventory_set_slot_tool(&inv, 0, TOOL_ID_3D_PRINTER);
    cfg = preheat_plan_config_default();
    cfg.enabled = true;
    const preheat_job_t print_job[] = { { TOOL_ID_3D_PRINTER, 1000u } };
    TEST_ASSERT(cfg.printer_target_c == 0u && preheat_plan_target_c(&cfg, TOOL_ID_3D_PRINTER) == 0u && preheat_plan_default_target_c(TOOL_ID_3D_PRINTER) == 0u, "a hotend has no target by default");
    TEST_ASSERT(preheat_plan_decide(&cfg, &inv, print_job, 1u, orders) == 0u, "default: the rack does not pre-heat a hotend");
    cfg.printer_target_c = 200u;
    TEST_ASSERT(preheat_plan_target_c(&cfg, TOOL_ID_3D_PRINTER) == 200u && preheat_plan_target_c(NULL, TOOL_ID_3D_PRINTER) == 0u, "the configured target");
    n = preheat_plan_decide(&cfg, &inv, print_job, 1u, orders);
    TEST_ASSERT(n == 1u && orders[0].tool_id == TOOL_ID_3D_PRINTER && orders[0].target_c == 200u, "a configured hotend is pre-heated before its job");
    cfg.printer_target_c = 500u;
    TEST_ASSERT(preheat_plan_target_c(&cfg, TOOL_ID_3D_PRINTER) == 310u, "never above the 310 degree ceiling");
    n = preheat_plan_decide(&cfg, &inv, print_job, 1u, orders);
    TEST_ASSERT(n == 1u && orders[0].target_c == 310u, "the order is clamped too");
    TEST_ASSERT(preheat_plan_lead_ms(TOOL_ID_3D_PRINTER, 200u, 0u) == 35000u, "175 degrees at 20 s per 100");
    TEST_ASSERT(preheat_plan_target_c(&cfg, TOOL_ID_SOLDERING_IRON) == 200u && preheat_plan_target_c(&cfg, 3u) == 0u, "other tools keep their targets");
}

void run_rack_units_tests(int *failures)
{
    test_identity_in_the_inventory(failures);
    test_units_and_counters(failures);
    test_slot_verify(failures);
    test_preheat_plan(failures);
}
