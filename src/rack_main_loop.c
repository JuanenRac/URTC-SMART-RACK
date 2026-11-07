// =============================================================================
// URTC-SMART-RACK Firmware - The pieces of the main loop joined in one context: rack_main_loop.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "rack_main_loop.h"

void rack_main_init(rack_ctx_t *ctx)
{
    rack_inventory_init(&ctx->inventory);
    link_watchdog_init(&ctx->watchdog, LINK_WATCHDOG_DEFAULT_TIMEOUT_MS);
    preheat_limit_init(&ctx->limit, RACK_PREHEAT_MAX_MS);
    rack_rx_init(&ctx->rx, 0u);
    ctx->now_ms = 0u;
}

bool rack_main_set_time(rack_ctx_t *ctx, uint32_t now_ms)
{
    if (now_ms < ctx->now_ms) {
        return false;
    }
    ctx->now_ms = now_ms;
    return true;
}

uint32_t rack_main_poll_serial(rack_ctx_t *ctx, rack_reply_fn reply, void *user)
{
    rack_decision_t decision;
    uint32_t handled = 0u;
    while (rack_rx_poll(&ctx->rx, &ctx->watchdog, ctx->now_ms, &decision)) {
        (void)rack_app_apply_decision_limited(&ctx->inventory, &ctx->limit, &decision, ctx->now_ms);
        if (reply != NULL) {
            reply(&decision, false, user);
        }
        handled++;
    }
    return handled;
}

void rack_main_on_can(rack_ctx_t *ctx, uint32_t can_id, const uint8_t *data, uint8_t dlc, rack_reply_fn reply, void *user)
{
    rack_decision_t decision;
    rack_link_decide_can(&ctx->watchdog, can_id, data, dlc, ctx->now_ms, &decision);
    (void)rack_app_apply_decision_limited(&ctx->inventory, &ctx->limit, &decision, ctx->now_ms);
    if (reply != NULL) {
        reply(&decision, true, user);
    }
}

bool rack_main_tick(rack_ctx_t *ctx)
{
    return rack_app_tick_limited(&ctx->inventory, &ctx->watchdog, &ctx->limit, ctx->now_ms);
}

bool rack_main_step(rack_ctx_t *ctx, uint32_t now_ms, rack_reply_fn reply, void *user)
{
    (void)rack_main_set_time(ctx, now_ms);
    (void)rack_main_poll_serial(ctx, reply, user);
    return rack_main_tick(ctx);
}
