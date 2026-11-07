// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_main_loop.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The context that main.c and the virtual rack share: bytes in through the receive ring, decisions out through the callback, time that never goes back.
#include <string.h>

#include "test_runner.h"
#include "../src/preheat.h"
#include "../src/protocol.h"
#include "../src/rack_command.h"
#include "../src/rack_main_loop.h"
#include "../src/tool_limits.h"

typedef struct {
    int serial_replies;
    int can_replies;
} counts_t;

static void count_reply(const rack_decision_t *decision, bool from_can, void *user)
{
    counts_t *c = (counts_t *)user;
    (void)decision;
    if (from_can) {
        c->can_replies++;
    } else {
        c->serial_replies++;
    }
}

static uint8_t encode_preheat(uint8_t seq, uint8_t tool, uint16_t temp, uint8_t *out)
{
    uint8_t payload[3] = {tool, (uint8_t)(temp >> 8), (uint8_t)(temp & 0xFFu)};
    return protocol_encode_frame(seq, RACK_CMD_SET_PREHEAT, payload, 3, out);
}

static uint16_t target_of(const rack_ctx_t *ctx, uint8_t slot)
{
    rack_slot_t s;
    return rack_inventory_get_slot(&ctx->inventory, slot, &s) ? s.target_temp_c : 0xFFFFu;
}

void run_rack_main_loop_tests(int *failures)
{
    rack_ctx_t ctx;
    counts_t counts = {0, 0};
    uint8_t frame[PROTOCOL_MAX_FRAME_SIZE];

    rack_main_init(&ctx);
    TEST_ASSERT(ctx.now_ms == 0u, "the clock starts at zero");
    rack_inventory_set_slot_tool(&ctx.inventory, 0, TOOL_ID_SOLDERING_IRON);

    // Time never goes back.
    TEST_ASSERT(rack_main_set_time(&ctx, 100u) && ctx.now_ms == 100u, "the clock moves forward");
    TEST_ASSERT(!rack_main_set_time(&ctx, 50u) && ctx.now_ms == 100u, "an earlier time is ignored");
    TEST_ASSERT(rack_main_set_time(&ctx, 100u), "the same time is accepted");

    // Without a reply callback the loop still works.
    TEST_ASSERT(rack_main_poll_serial(&ctx, NULL, NULL) == 0u, "an empty ring handles no frame, and a missing callback is fine");

    // A frame in through the ring comes out as one reply and one target.
    const uint8_t len = encode_preheat(1, TOOL_ID_SOLDERING_IRON, 200, frame);
    for (uint8_t i = 0; i < len; i++) {
        (void)rack_rx_on_byte(&ctx.rx, frame[i]);
    }
    TEST_ASSERT(rack_main_poll_serial(&ctx, count_reply, &counts) == 1u && counts.serial_replies == 1 && counts.can_replies == 0, "one frame, one serial reply");
    TEST_ASSERT(target_of(&ctx, 0) == 200, "the target of the slot is set");

    // The tick holds the safe state once the link is lost (no frame for longer than the timeout).
    TEST_ASSERT(!rack_main_tick(&ctx), "the link is alive right after a frame");
    TEST_ASSERT(rack_main_set_time(&ctx, 100u + LINK_WATCHDOG_DEFAULT_TIMEOUT_MS + 1u), "time passes");
    TEST_ASSERT(rack_main_tick(&ctx) && target_of(&ctx, 0) == preheat_safe_state_temp_c(), "after the timeout the link is lost and the slot is in the safe state");

    // One whole pass: time, bytes, tick.
    rack_main_init(&ctx);
    rack_inventory_set_slot_tool(&ctx.inventory, 0, TOOL_ID_SOLDERING_IRON);
    memset(&counts, 0, sizeof counts);
    const uint8_t len2 = encode_preheat(1, TOOL_ID_SOLDERING_IRON, 180, frame);
    for (uint8_t i = 0; i < len2; i++) {
        (void)rack_rx_on_byte(&ctx.rx, frame[i]);
    }
    TEST_ASSERT(!rack_main_step(&ctx, 10u, count_reply, &counts) && counts.serial_replies == 1 && target_of(&ctx, 0) == 180, "a pass handles the frame and leaves the link alive");
    TEST_ASSERT(rack_main_step(&ctx, 10u + LINK_WATCHDOG_DEFAULT_TIMEOUT_MS + 5u, count_reply, &counts) && target_of(&ctx, 0) == preheat_safe_state_temp_c(), "a later pass sees the loss");

    // A CAN frame goes through the same decision and answers through the callback as CAN.
    const uint8_t can_data[1] = {0};
    rack_main_on_can(&ctx, 0x7FFu, can_data, 1, count_reply, &counts);
    TEST_ASSERT(counts.can_replies == 1, "a CAN frame is handed to the callback as CAN, even when the decision is to do nothing");
    rack_main_on_can(&ctx, 0x7FFu, can_data, 1, NULL, NULL);
    TEST_ASSERT(counts.can_replies == 1, "a missing callback is fine for CAN too");
}
