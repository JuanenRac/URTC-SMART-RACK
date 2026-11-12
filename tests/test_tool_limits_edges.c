// =============================================================================
// URTC-SMART-RACK Firmware - edge tests for tool_limits.c and slot_verify.c (every threshold one step either side)
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The ceiling of each tool over every possible tool id, and the verdict of a slot at each of its thresholds: exactly on it and one step beyond it, with the readings a sensor can
// give that are not numbers (not a number, infinite).
#include <math.h>
#include <stdint.h>

#include "../src/slot_verify.h"
#include "../src/tool_id.h"
#include "../src/tool_limits.h"
#include "test_runner.h"

void run_tool_limits_edge_tests(int *failures)
{
    // Over all 256 values of a tool id: only the three heated tools have a ceiling, and none is above the ceiling of the rack.
    unsigned with_ceiling = 0u;
    for (unsigned id = 0u; id < 256u; id++) {
        const uint16_t limit = tool_limits_max_preheat_c((uint8_t)id);
        if (limit != 0u) {
            with_ceiling++;
            TEST_ASSERT(limit <= TOOL_LIMITS_RACK_CEILING_C, "no ceiling is above the ceiling of the rack");
            TEST_ASSERT(tool_id_is_valid_urtc((uint8_t)id), "only a valid tool id has a ceiling");
            TEST_ASSERT(id == TOOL_ID_SOLDERING_IRON || id == TOOL_ID_3D_PRINTER || id == TOOL_ID_HOTAIR_REWORK, "only the heated tools have a ceiling");
        } else if (!tool_id_is_valid_urtc((uint8_t)id)) {
            TEST_ASSERT(limit == 0u, "an id that is not a tool has no ceiling");
        }
    }
    TEST_ASSERT(with_ceiling == 3u, "exactly three tools have a pre-heat ceiling");
    TEST_ASSERT(tool_limits_max_preheat_c(TOOL_ID_3D_PRINTER) == 310u, "a hotend is held to 310 degrees");
    TEST_ASSERT(tool_limits_max_preheat_c(TOOL_ID_URTC_LAST) <= TOOL_LIMITS_RACK_CEILING_C, "the last tool is inside the ceiling");
    TEST_ASSERT(tool_limits_max_preheat_c((uint8_t)(TOOL_ID_URTC_LAST + 1u)) == 0u && tool_limits_max_preheat_c(255u) == 0u, "the ids after the last tool have none");

    // An empty slot: hot only strictly above ambient + 40.
    TEST_ASSERT(slot_verify(false, 0u, 65.0f, 25.0f, 0u) == SLOT_VERDICT_EMPTY_OK, "an empty slot exactly 40 above ambient is still fine");
    TEST_ASSERT(slot_verify(false, 0u, 65.1f, 25.0f, 0u) == SLOT_VERDICT_EMPTY_HOT, "a little above that it is a heater on with no tool");
    TEST_ASSERT(slot_verify(false, 300u, 25.0f, 25.0f, 0u) == SLOT_VERDICT_EMPTY_OK, "the target is not looked at when the slot is empty");
    TEST_ASSERT(slot_verify(false, 0u, -40.0f, 25.0f, 0u) == SLOT_VERDICT_EMPTY_OK, "a cold reading is fine");

    // A tool told to stay cold: hot only strictly above ambient + 60.
    TEST_ASSERT(slot_verify(true, 0u, 85.0f, 25.0f, 0u) == SLOT_VERDICT_IDLE_OK, "exactly 60 above ambient is still idle");
    TEST_ASSERT(slot_verify(true, 0u, 85.1f, 25.0f, 0u) == SLOT_VERDICT_IDLE_HOT, "a little above that it is hot while told to stay cold");

    // A tool with a target: the tolerance below, the overshoot above, and the time to get there.
    TEST_ASSERT(slot_verify(true, 200u, 185.0f, 25.0f, 0u) == SLOT_VERDICT_REACHED, "exactly 15 below the target is reached");
    TEST_ASSERT(slot_verify(true, 200u, 184.9f, 25.0f, 0u) == SLOT_VERDICT_HEATING, "a little below that it is still heating");
    TEST_ASSERT(slot_verify(true, 200u, 225.0f, 25.0f, 0u) == SLOT_VERDICT_REACHED, "exactly 25 above the target is still reached");
    TEST_ASSERT(slot_verify(true, 200u, 225.1f, 25.0f, 0u) == SLOT_VERDICT_OVERSHOOT, "a little above that it is too hot");
    TEST_ASSERT(slot_verify(true, 200u, 100.0f, 25.0f, SLOT_VERIFY_REACH_TIMEOUT_MS - 1u) == SLOT_VERDICT_HEATING, "one millisecond before the time is up it is still heating");
    TEST_ASSERT(slot_verify(true, 200u, 100.0f, 25.0f, SLOT_VERIFY_REACH_TIMEOUT_MS) == SLOT_VERDICT_NOT_REACHED, "when the time is up it has not reached the target");
    TEST_ASSERT(slot_verify(true, 200u, 100.0f, 25.0f, 0xFFFFFFFFu) == SLOT_VERDICT_NOT_REACHED, "the largest elapsed time does not wrap round");
    TEST_ASSERT(slot_verify(true, 200u, 190.0f, 25.0f, 0xFFFFFFFFu) == SLOT_VERDICT_REACHED, "a tool that got there is reached whatever the time");
    TEST_ASSERT(slot_verify(true, 1u, 0.0f, 25.0f, 0u) == SLOT_VERDICT_REACHED, "a target of one degree is reached from zero (inside the tolerance)");
    TEST_ASSERT(slot_verify(true, 65535u, 100.0f, 25.0f, 0u) == SLOT_VERDICT_HEATING, "the largest target is a float and does not wrap round");

    // Readings that are not numbers.
    TEST_ASSERT(slot_verify(true, 200u, NAN, 25.0f, 0u) == SLOT_VERDICT_UNKNOWN && slot_verify(false, 0u, NAN, 25.0f, 0u) == SLOT_VERDICT_UNKNOWN, "not a number is no reading, occupied or not");
    TEST_ASSERT(slot_verify(true, 0u, NAN, 25.0f, 0u) == SLOT_VERDICT_UNKNOWN, "not a number is no reading when told to stay cold");
    TEST_ASSERT(slot_verify(true, 200u, INFINITY, 25.0f, 0u) == SLOT_VERDICT_OVERSHOOT, "an infinite reading is too hot");
    TEST_ASSERT(slot_verify(false, 0u, INFINITY, 25.0f, 0u) == SLOT_VERDICT_EMPTY_HOT && slot_verify(true, 0u, INFINITY, 25.0f, 0u) == SLOT_VERDICT_IDLE_HOT, "and so in an empty slot and in an idle one");
    TEST_ASSERT(slot_verify(true, 200u, -INFINITY, 25.0f, 0u) == SLOT_VERDICT_HEATING && slot_verify(true, 200u, -INFINITY, 25.0f, 60000u) == SLOT_VERDICT_NOT_REACHED, "a reading of minus infinity is cold");

    // The faults are exactly four of the nine verdicts, and every verdict is told apart.
    unsigned faults = 0u;
    for (int verdict = (int)SLOT_VERDICT_UNKNOWN; verdict <= (int)SLOT_VERDICT_OVERSHOOT; verdict++) {
        faults += slot_verdict_is_fault((slot_verdict_t)verdict) ? 1u : 0u;
    }
    TEST_ASSERT(faults == 4u, "four verdicts are faults: empty hot, idle hot, not reached and overshoot");
    TEST_ASSERT(!slot_verdict_is_fault(SLOT_VERDICT_UNKNOWN) && !slot_verdict_is_fault(SLOT_VERDICT_REACHED) && !slot_verdict_is_fault(SLOT_VERDICT_HEATING),
                "no reading, heating and reached are not faults");
    TEST_ASSERT(!slot_verdict_is_fault((slot_verdict_t)99), "a value that is not a verdict is not a fault");

    // The verdict never depends on the previous call (no hidden state): the same arguments give the same answer in any order.
    const slot_verdict_t a = slot_verify(true, 200u, 190.0f, 25.0f, 10u);
    (void)slot_verify(true, 200u, 500.0f, 25.0f, 99999u);
    TEST_ASSERT(slot_verify(true, 200u, 190.0f, 25.0f, 10u) == a, "the same arguments give the same verdict");
}
