// =============================================================================
// URTC-SMART-RACK Firmware - tests for free_tool.c (heads whose ID jumpers read 0x1F)
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The shared vectors of URTC-COMMON are replayed in test_rack_vectors.c; this file checks the rules of the contract one by one and the way
// the slots of the rack behave around them (no target, no pre-heat order while a head has no identity).
#include "../src/free_tool.h"
#include "../src/preheat_plan.h"
#include "../src/rack_inventory.h"
#include "../src/tool_id.h"
#include "../src/tool_limits.h"
#include "test_runner.h"

static const uint8_t ANSWER_TOOL_3[] = {0x1F, 0x04};
static const uint8_t ANSWER_TOOL_SOLDERING[] = {0x1F, 0x01};
static const uint8_t ANSWER_TOOL_PRINTER[] = {0x1F, 0x0B};
static const uint8_t ANSWER_NOT_SET[] = {0x1F, 0x00};

void run_free_tool_tests(int *failures)
{
    uint8_t tool = 99;

    // The jumper readings.
    TEST_ASSERT(free_tool_classify_jumpers(0x00, &tool) == FREE_TOOL_KIND_DIRECT && tool == 0, "jumpers 0 are tool 0");
    TEST_ASSERT(free_tool_classify_jumpers(0x18, &tool) == FREE_TOOL_KIND_DIRECT && tool == 24, "jumpers 0x18 are the last tool");
    TEST_ASSERT(free_tool_classify_jumpers(0x19, NULL) == FREE_TOOL_KIND_INVALID, "jumpers 0x19 name no tool");
    TEST_ASSERT(free_tool_classify_jumpers(0x1E, NULL) == FREE_TOOL_KIND_INVALID, "jumpers 0x1E name no tool");
    TEST_ASSERT(free_tool_classify_jumpers(0x1F, NULL) == FREE_TOOL_KIND_FREE, "jumpers 0x1F are free configuration");
    TEST_ASSERT(free_tool_classify_jumpers(0xFF, NULL) == FREE_TOOL_KIND_FREE, "bits above the fifth are ignored");

    // The answers.
    TEST_ASSERT(free_tool_parse_reply(ANSWER_TOOL_3, 2, &tool) && tool == 3, "an answer names the tool id plus one");
    TEST_ASSERT(!free_tool_parse_reply(ANSWER_NOT_SET, 2, &tool), "selection 0 names no tool");
    TEST_ASSERT(!free_tool_parse_reply(ANSWER_TOOL_3, 1, &tool), "a one-byte answer names no tool");
    TEST_ASSERT(!free_tool_parse_reply(NULL, 2, &tool), "no data names no tool");
    {
        const uint8_t not_free[] = {0x0A, 0x04};
        const uint8_t too_high[] = {0x1F, 0x1A};
        const uint8_t last[] = {0x1F, 0x19};
        TEST_ASSERT(!free_tool_parse_reply(not_free, 2, &tool), "a head whose jumpers do not read 0x1F is not in free mode");
        TEST_ASSERT(!free_tool_parse_reply(too_high, 2, &tool), "selection 26 names no tool");
        TEST_ASSERT(free_tool_parse_reply(last, 2, &tool) && tool == 24, "selection 25 is the last tool");
    }

    // The defaults are the ones of the contract and nothing else is hard-coded in the logic: a short timeout changes the behaviour.
    {
        const free_tool_config_t d = free_tool_config_default();
        TEST_ASSERT(d.query_timeout_ms == 250u && d.max_attempts == 3u && d.retry_backoff_ms == 2000u && d.refresh_period_ms == 5000u,
                    "default query timing");
        free_tool_config_t fast = d;
        fast.query_timeout_ms = 10u;
        fast.max_attempts = 2u;
        fast.retry_backoff_ms = 30u;
        free_tool_t ft;
        free_tool_reset(&ft);
        TEST_ASSERT(free_tool_step(&ft, &fast, 0u, NULL, 0, NULL), "first step asks at once");
        TEST_ASSERT(free_tool_step(&ft, &fast, 10u, NULL, 0, NULL), "second query after the timeout");
        TEST_ASSERT(!free_tool_step(&ft, &fast, 20u, NULL, 0, NULL), "round over after the configured attempts");
        TEST_ASSERT(!free_tool_step(&ft, &fast, 49u, NULL, 0, NULL) && free_tool_step(&ft, &fast, 50u, NULL, 0, NULL), "the configured back-off");
    }

    // A slot with a head in free mode: no identity, no target, no pre-heat order, until the answer arrives.
    {
        const free_tool_config_t cfg = free_tool_config_default();
        rack_inventory_t inv;
        free_tool_bank_t bank;
        free_tool_event_t event;
        rack_inventory_init(&inv);
        free_tool_bank_init(&bank);

        TEST_ASSERT(free_tool_bank_set_jumpers(&bank, &inv, 2, 0x1F) == FREE_TOOL_KIND_FREE, "a free head is recognised");
        TEST_ASSERT(free_tool_bank_identity(&bank, &inv, 2) == FREE_TOOL_ID_PENDING, "a free head has no identity yet");
        TEST_ASSERT(!inv.slots[2].occupied, "the slot of a free head holds no tool, not even 'none' by assumption of a tool");
        TEST_ASSERT(!rack_inventory_set_preheat_target(&inv, TOOL_ID_SOLDERING_IRON, 250), "no target for a tool that is not known");

        preheat_plan_config_t plan = preheat_plan_config_default();
        plan.enabled = true;
        const preheat_job_t jobs[] = {{TOOL_ID_SOLDERING_IRON, 0u}};
        preheat_order_t orders[PREHEAT_PLAN_MAX_ORDERS];
        TEST_ASSERT(preheat_plan_decide(&plan, &inv, jobs, 1, orders) == 0, "no pre-heat order while the head has no identity");

        TEST_ASSERT(free_tool_bank_step(&bank, &cfg, &inv, 2, 0u, NULL, 0, &event) && event == FREE_TOOL_EVT_NONE, "the head is asked");
        TEST_ASSERT(!free_tool_bank_step(&bank, &cfg, &inv, 2, 100u, ANSWER_TOOL_SOLDERING, 2, &event) && event == FREE_TOOL_EVT_RESOLVED,
                    "a valid answer resolves it");
        TEST_ASSERT(free_tool_bank_identity(&bank, &inv, 2) == FREE_TOOL_ID_TOOL && inv.slots[2].tool_id == TOOL_ID_SOLDERING_IRON,
                    "the slot now holds the tool of the head, like a head read from jumpers");
        TEST_ASSERT(rack_inventory_set_preheat_target(&inv, TOOL_ID_SOLDERING_IRON, 250), "the same flow as a jumper head: a target is accepted");
        TEST_ASSERT(preheat_plan_decide(&plan, &inv, jobs, 1, orders) == 1, "and the plan can order its pre-heat");

        // The same answer again confirms and keeps the target.
        (void)free_tool_bank_step(&bank, &cfg, &inv, 2, 5100u, NULL, 0, &event);
        (void)free_tool_bank_step(&bank, &cfg, &inv, 2, 5110u, ANSWER_TOOL_SOLDERING, 2, &event);
        TEST_ASSERT(event == FREE_TOOL_EVT_CONFIRMED && inv.slots[2].target_temp_c == 250u, "the same tool confirms and changes nothing");

        // A different answer is a change of tool: the target is dropped.
        (void)free_tool_bank_step(&bank, &cfg, &inv, 2, 10110u, NULL, 0, &event);
        (void)free_tool_bank_step(&bank, &cfg, &inv, 2, 10120u, ANSWER_TOOL_PRINTER, 2, &event);
        TEST_ASSERT(event == FREE_TOOL_EVT_CHANGED && inv.slots[2].tool_id == TOOL_ID_3D_PRINTER && inv.slots[2].target_temp_c == 0u,
                    "another tool is a change of tool: the new tool, no target");
        TEST_ASSERT(!rack_inventory_set_preheat_target(&inv, TOOL_ID_SOLDERING_IRON, 250), "the old tool is no longer racked");

        // The answer is withdrawn: the slot is emptied and nothing can be ordered.
        (void)free_tool_bank_step(&bank, &cfg, &inv, 2, 15120u, NULL, 0, &event);
        (void)free_tool_bank_step(&bank, &cfg, &inv, 2, 15130u, ANSWER_NOT_SET, 2, &event);
        TEST_ASSERT(event == FREE_TOOL_EVT_LOST && !inv.slots[2].occupied && free_tool_bank_identity(&bank, &inv, 2) == FREE_TOOL_ID_PENDING,
                    "an answer that names no tool withdraws the identity");

        // Silence is the safe state too: a head that was resolved and then goes quiet for a round is withdrawn.
        free_tool_bank_init(&bank);
        rack_inventory_init(&inv);
        (void)free_tool_bank_set_jumpers(&bank, &inv, 0, 0x1F);
        (void)free_tool_bank_step(&bank, &cfg, &inv, 0, 0u, NULL, 0, &event);
        (void)free_tool_bank_step(&bank, &cfg, &inv, 0, 10u, ANSWER_TOOL_3, 2, &event);
        TEST_ASSERT(inv.slots[0].occupied && inv.slots[0].tool_id == 3u, "resolved");
        TEST_ASSERT(free_tool_bank_step(&bank, &cfg, &inv, 0, 5010u, NULL, 0, &event), "refresh query");
        TEST_ASSERT(free_tool_bank_step(&bank, &cfg, &inv, 0, 5260u, NULL, 0, &event), "refresh retry");
        TEST_ASSERT(free_tool_bank_step(&bank, &cfg, &inv, 0, 5510u, NULL, 0, &event), "refresh last retry");
        TEST_ASSERT(!free_tool_bank_step(&bank, &cfg, &inv, 0, 5760u, NULL, 0, &event) && event == FREE_TOOL_EVT_LOST && !inv.slots[0].occupied,
                    "a whole round of silence withdraws the identity");
    }

    // Direct and invalid jumpers never ask anything.
    {
        const free_tool_config_t cfg = free_tool_config_default();
        rack_inventory_t inv;
        free_tool_bank_t bank;
        free_tool_event_t event;
        rack_inventory_init(&inv);
        free_tool_bank_init(&bank);
        TEST_ASSERT(free_tool_bank_set_jumpers(&bank, &inv, 1, 0x0A) == FREE_TOOL_KIND_DIRECT && inv.slots[1].tool_id == TOOL_ID_3D_PRINTER,
                    "a jumper head is racked at once");
        TEST_ASSERT(!free_tool_bank_step(&bank, &cfg, &inv, 1, 0u, NULL, 0, &event), "a jumper head is never asked");
        TEST_ASSERT(free_tool_bank_set_jumpers(&bank, &inv, 3, 0x1E) == FREE_TOOL_KIND_INVALID && free_tool_bank_identity(&bank, &inv, 3) == FREE_TOOL_ID_INVALID,
                    "unassigned jumpers give no identity");
        TEST_ASSERT(!free_tool_bank_step(&bank, &cfg, &inv, 3, 0u, NULL, 0, &event), "and are never asked");
        TEST_ASSERT(free_tool_bank_set_jumpers(&bank, &inv, RACK_INVENTORY_MAX_SLOTS, 0x00) == FREE_TOOL_KIND_INVALID, "a slot out of range is refused");
        // A head swapped for a jumper head in the same slot leaves free mode.
        (void)free_tool_bank_set_jumpers(&bank, &inv, 4, 0x1F);
        (void)free_tool_bank_set_jumpers(&bank, &inv, 4, 0x05);
        TEST_ASSERT(free_tool_bank_identity(&bank, &inv, 4) == FREE_TOOL_ID_TOOL && !free_tool_bank_step(&bank, &cfg, &inv, 4, 0u, NULL, 0, &event),
                    "a slot that changes to a jumper head stops asking");
    }
}
