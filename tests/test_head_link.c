// =============================================================================
// URTC-SMART-RACK Firmware - Host-side tests: the frames between the rack and the head in its slot (test_head_link.c)
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include <string.h>

#include "../src/head_link.h"
#include "../src/rack_units.h"
#include "test_runner.h"

static void test_the_questions_have_no_payload(int *failures)
{
    head_frame_t frame;
    memset(&frame, 0xAA, sizeof(frame));
    TEST_ASSERT(head_link_build_query_active_tool(&frame) && frame.id == 0x110u && frame.dlc == 0u, "the question about the active tool is 0x110 with no payload");
    TEST_ASSERT(head_link_build_query_counters(&frame) && frame.id == 0x1AAu && frame.dlc == 0u && frame.data[0] == 0u, "the question about the counters is 0x1AA with no payload");
    TEST_ASSERT(!head_link_build_query_active_tool(NULL) && !head_link_build_query_counters(NULL), "no frame can be built into nothing");
}

static void test_the_setpoint_is_the_ordinary_frame_of_the_tool(int *failures)
{
    const head_link_config_t config = head_link_config_default();
    head_frame_t frame;
    TEST_ASSERT(head_link_build_setpoint(&config, URTC_TOOL_ID_SOLDERING_IRON, 200u, &frame), "the soldering iron has a setpoint frame");
    TEST_ASSERT(frame.id == 0x130u && frame.dlc == 2u && frame.data[0] == 0x00u && frame.data[1] == 0xC8u, "0x130 carries the target as a big-endian number");
    TEST_ASSERT(head_link_build_setpoint(&config, URTC_TOOL_ID_SOLDERING_IRON, 0x0123u, &frame) && frame.data[0] == 0x01u && frame.data[1] == 0x23u, "both bytes of the target are used");
    TEST_ASSERT(head_link_build_setpoint(&config, URTC_TOOL_ID_HOTAIR_REWORK, 320u, &frame), "hot air has a setpoint frame");
    TEST_ASSERT(frame.id == 0x1E0u && frame.dlc == 3u && frame.data[0] == 0x01u && frame.data[1] == 0x40u && frame.data[2] == HEAD_LINK_DEFAULT_HOTAIR_BLOWER_DUTY,
                "0x1E0 carries the target and the blower duty of the configuration");
    head_link_config_t other = config;
    other.hotair_blower_duty = 200u;
    TEST_ASSERT(head_link_build_setpoint(&other, URTC_TOOL_ID_HOTAIR_REWORK, 0u, &frame) && frame.data[2] == 200u && frame.data[0] == 0u && frame.data[1] == 0u,
                "a target of 0 is a frame too (the order to cool) and the duty is the configured one");
    TEST_ASSERT(!head_link_build_setpoint(&config, 1u, 100u, &frame) && !head_link_build_setpoint(&config, 24u, 100u, &frame), "a tool the rack does not warm up has no frame");
    TEST_ASSERT(!head_link_build_setpoint(NULL, 0u, 1u, &frame) && !head_link_build_setpoint(&config, 0u, 1u, NULL), "a missing argument builds nothing");
}

static void test_the_answer_about_the_active_tool(int *failures)
{
    head_state_t state;
    const uint8_t full[6] = {19u, 0u, 1u, 0u, 7u, 3u};
    TEST_ASSERT(head_link_parse_active_tool(0x111u, full, 6u, &state), "a full answer is read");
    TEST_ASSERT(state.tool_id == 19u && !state.system_error && state.can_error && !state.booting && state.thermal_fault == 7u && state.reset_cause == 3u, "every field is where the specification puts it");
    TEST_ASSERT(head_link_parse_active_tool(0x111u, full, 4u, &state) && state.thermal_fault == 0xFFu && state.reset_cause == 0xFFu, "a four-byte answer says nothing of the fault and the reset cause");
    TEST_ASSERT(head_link_parse_active_tool(0x111u, full, 5u, &state) && state.thermal_fault == 7u && state.reset_cause == 0xFFu, "a five-byte answer has the fault only");
    state.tool_id = 99u;
    TEST_ASSERT(!head_link_parse_active_tool(0x111u, full, 3u, &state) && state.tool_id == 99u, "a short answer is refused and writes nothing");
    TEST_ASSERT(!head_link_parse_active_tool(0x110u, full, 6u, &state) && !head_link_parse_active_tool(0x111u, NULL, 6u, &state), "another identifier or no data is not an answer");
}

static void test_the_answer_about_the_counters_feeds_the_wear_of_the_unit(int *failures)
{
    head_counters_t counters;
    const uint8_t data[8] = {0x00u, 0x03u, 0x00u, 0x01u, 0x51u, 0x80u, 0x05u, 0x01u};   // 3 faults, 86400 s of heater, last fault 5, kept in F-RAM
    TEST_ASSERT(head_link_parse_counters(0x1ABu, data, 8u, &counters), "the answer is read");
    TEST_ASSERT(counters.fault_count == 3u && counters.heater_on_seconds == 86400u && counters.last_fault == 5u && counters.persisted, "fault count, heater seconds, last fault and persistence");
    TEST_ASSERT(!head_link_parse_counters(0x1ABu, data, 7u, &counters) && !head_link_parse_counters(0x1AAu, data, 8u, &counters), "another length or identifier is not an answer");
    rack_units_t units;
    rack_units_init(&units);
    TEST_ASSERT(rack_units_note_counters(&units, URTC_TOOL_ID_SOLDERING_IRON, 7u, counters.fault_count, counters.heater_on_seconds, counters.last_fault, counters.persisted),
                "the numbers go to the record of the unit");
    rack_unit_t *unit = rack_units_find(&units, URTC_TOOL_ID_SOLDERING_IRON, 7u);
    TEST_ASSERT(unit != NULL && unit->heater_seconds == 86400u && unit->faults == 3u && unit->last_fault == 5u, "the unit holds what the head reported");
}

static void test_the_setpoint_is_sent_again_while_the_order_stands(int *failures)
{
    head_link_config_t config = head_link_config_default();
    head_setpoint_state_t state;
    head_link_setpoint_reset(&state);
    TEST_ASSERT(head_link_setpoint_due(&config, &state, 200u, 5000u), "the first one goes at once");
    TEST_ASSERT(!head_link_setpoint_due(&config, &state, 200u, 5500u), "not again before the refresh time");
    TEST_ASSERT(head_link_setpoint_due(&config, &state, 200u, 5000u + HEAD_LINK_DEFAULT_REFRESH_MS), "again when the refresh time has passed");
    TEST_ASSERT(head_link_setpoint_due(&config, &state, 150u, 6100u), "a changed target goes at once");
    TEST_ASSERT(head_link_setpoint_due(&config, &state, 0u, 6200u), "a target of 0 goes at once too (cool)");
    TEST_ASSERT(!head_link_setpoint_due(&config, &state, 0u, 6300u) && head_link_setpoint_due(&config, &state, 0u, 7200u), "and is refreshed as the others");
    head_link_setpoint_reset(&state);
    TEST_ASSERT(head_link_setpoint_due(&config, &state, 0u, 7201u), "after a reset (the head left, the link was lost) the next one goes at once");
    head_link_setpoint_reset(&state);
    TEST_ASSERT(head_link_setpoint_due(&config, &state, 100u, 0xFFFFFF00u), "near the end of the clock");
    TEST_ASSERT(!head_link_setpoint_due(&config, &state, 100u, 0xFFFFFF00u + 500u), "the time before the wrap");
    TEST_ASSERT(head_link_setpoint_due(&config, &state, 100u, (uint32_t)(0xFFFFFF00u + HEAD_LINK_DEFAULT_REFRESH_MS)), "a refresh across the wrap of the clock is still found");
    TEST_ASSERT(!head_link_setpoint_due(NULL, &state, 1u, 0u) && !head_link_setpoint_due(&config, NULL, 1u, 0u), "a missing argument sends nothing");
    config.setpoint_refresh_ms = 250u;
    head_link_setpoint_reset(&state);
    head_link_setpoint_due(&config, &state, 80u, 1000u);
    TEST_ASSERT(head_link_setpoint_due(&config, &state, 80u, 1250u), "the refresh time is the configured one, not a constant");
}

void run_head_link_tests(int *failures)
{
    test_the_questions_have_no_payload(failures);
    test_the_setpoint_is_the_ordinary_frame_of_the_tool(failures);
    test_the_answer_about_the_active_tool(failures);
    test_the_answer_about_the_counters_feeds_the_wear_of_the_unit(failures);
    test_the_setpoint_is_sent_again_while_the_order_stands(failures);
}
