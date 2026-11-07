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

// ---- failures: a bit flipped at every position, a frame cut at every length, a ring that is full, a clock that jumps, and a storm of noise ----------------
static uint32_t noise_state = 0xC0FFEE11u;
static uint32_t noise_next(void)
{
    noise_state ^= noise_state << 13;
    noise_state ^= noise_state >> 17;
    noise_state ^= noise_state << 5;
    return noise_state;
}

static void fresh_rack(rack_ctx_t *ctx)
{
    rack_main_init(ctx);
    rack_inventory_set_slot_tool(&ctx->inventory, 0, TOOL_ID_SOLDERING_IRON);
}

static void fault_scenarios(int *failures)
{
    rack_ctx_t ctx;
    counts_t counts = {0, 0};
    uint8_t frame[PROTOCOL_MAX_FRAME_SIZE];
    const uint8_t len = encode_preheat(1, TOOL_ID_SOLDERING_IRON, 200, frame);

    // A frame with one bit flipped, anywhere: never applied (the checksum catches every single-bit error).
    int applied_after_flip = 0;
    for (uint8_t i = 0; i < len; i++) {
        for (uint8_t bit = 0; bit < 8; bit++) {
            fresh_rack(&ctx);
            const uint16_t before = target_of(&ctx, 0);
            for (uint8_t k = 0; k < len; k++) {
                (void)rack_rx_on_byte(&ctx.rx, (uint8_t)(k == i ? frame[k] ^ (uint8_t)(1u << bit) : frame[k]));
            }
            (void)rack_main_step(&ctx, 10u, count_reply, &counts);
            if (target_of(&ctx, 0) != before) {
                applied_after_flip++;
            }
        }
    }
    TEST_ASSERT(applied_after_flip == 0, "a frame with any single bit flipped never changes a target");

    // A frame cut at every length: nothing is applied until the last byte.
    for (uint8_t cut = 0; cut < len; cut++) {
        fresh_rack(&ctx);
        const uint16_t before = target_of(&ctx, 0);
        for (uint8_t k = 0; k < cut; k++) {
            (void)rack_rx_on_byte(&ctx.rx, frame[k]);
        }
        (void)rack_main_step(&ctx, 10u, NULL, NULL);
        TEST_ASSERT(target_of(&ctx, 0) == before, "a frame that is cut short changes nothing");
    }
    fresh_rack(&ctx);
    for (uint8_t k = 0; k < len; k++) {
        (void)rack_rx_on_byte(&ctx.rx, frame[k]);
    }
    (void)rack_main_step(&ctx, 10u, NULL, NULL);
    TEST_ASSERT(target_of(&ctx, 0) == 200, "the whole frame is applied");

    // A ring that nobody empties drops the bytes it has no room for and never writes beyond itself.
    fresh_rack(&ctx);
    int dropped = 0;
    for (int k = 0; k < 100; k++) {
        if (!rack_rx_on_byte(&ctx.rx, (uint8_t)k)) {
            dropped++;
        }
    }
    TEST_ASSERT(dropped >= 100 - (int)RACK_RX_RING_SIZE && dropped < 100, "a full ring drops the bytes it has no room for and keeps the rest");
    (void)rack_main_step(&ctx, 20u, NULL, NULL);
    TEST_ASSERT(target_of(&ctx, 0) != 200u, "bytes that are not a frame do not set a target");

    // The clock: an earlier time, the largest time and a step with a time in the past are all harmless.
    fresh_rack(&ctx);
    TEST_ASSERT(rack_main_set_time(&ctx, 0xFFFFFFFFu) && ctx.now_ms == 0xFFFFFFFFu, "the largest time is accepted");
    TEST_ASSERT(!rack_main_set_time(&ctx, 0u) && ctx.now_ms == 0xFFFFFFFFu, "a time that wrapped to zero is ignored, the clock never goes back");
    (void)rack_main_step(&ctx, 5u, NULL, NULL);
    TEST_ASSERT(ctx.now_ms == 0xFFFFFFFFu, "a step with a time in the past keeps the clock");

    // A storm of noise through the whole loop: no crash, no more frames than bytes, and the sanitizers see no bad access.
    fresh_rack(&ctx);
    uint32_t bytes = 0u;
    uint32_t handled = 0u;
    uint32_t now = 1u;
    for (int i = 0; i < 20000; i++) {
        const uint32_t burst = noise_next() % 12u;
        for (uint32_t k = 0; k < burst; k++) {
            (void)rack_rx_on_byte(&ctx.rx, (uint8_t)noise_next());
            bytes++;
        }
        now += noise_next() % 40u;
        (void)rack_main_set_time(&ctx, now);
        handled += rack_main_poll_serial(&ctx, NULL, NULL);
        (void)rack_main_tick(&ctx);
    }
    TEST_ASSERT(handled <= bytes, "the loop never reports more frames than bytes it was given");

    // After the noise, a good frame in a clean rack still works, and the link watchdog sees it.
    fresh_rack(&ctx);
    for (uint8_t k = 0; k < len; k++) {
        (void)rack_rx_on_byte(&ctx.rx, frame[k]);
    }
    TEST_ASSERT(!rack_main_step(&ctx, 1u, NULL, NULL) && target_of(&ctx, 0) == 200, "a good frame is applied and the link is alive");
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

    fault_scenarios(failures);
}
