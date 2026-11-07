// =============================================================================
// URTC-SMART-RACK Firmware - The pieces of the main loop joined in one context: rack_main_loop.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// main.c and the virtual rack of the tests (tests/virtual_rack.c) run exactly this code: the receive ring, the deframer, the link decision, the watchdog,
// the inventory and the pre-heat limit, joined the way the loop of the board joins them. Nothing here touches a peripheral: the board supplies the time
// and the bytes, and takes the replies through a callback.
//
//   interrupt (serial):  rack_rx_on_byte(&ctx.rx, byte);
//   main loop:           rack_main_step(&ctx, millis(), reply_cb, user);
#ifndef RACK_MAIN_LOOP_H
#define RACK_MAIN_LOOP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "link_watchdog.h"
#include "preheat_limit.h"
#include "rack_app.h"
#include "rack_inventory.h"
#include "rack_link.h"
#include "rack_rx.h"

typedef struct {
    rack_inventory_t inventory;
    link_watchdog_t watchdog;
    preheat_limit_t limit;
    rack_rx_t rx;
    uint32_t now_ms;
} rack_ctx_t;

// Called once for every decision made from a frame (serial or CAN); the reply is encoded from it by rack_reply.h / rack_can.h.
typedef void (*rack_reply_fn)(const rack_decision_t *decision, bool from_can, void *user);

void rack_main_init(rack_ctx_t *ctx);

// The clock is `now_ms` (never goes back; an earlier value is ignored and false is returned).
bool rack_main_set_time(rack_ctx_t *ctx, uint32_t now_ms);

// Takes the bytes the receive ring holds, applies each decision and hands it to `reply`. Returns the number of frames handled.
uint32_t rack_main_poll_serial(rack_ctx_t *ctx, rack_reply_fn reply, void *user);

// One CAN frame.
void rack_main_on_can(rack_ctx_t *ctx, uint32_t can_id, const uint8_t *data, uint8_t dlc, rack_reply_fn reply, void *user);

// The tick: watchdog, safe state while the link is lost, and the pre-heat limit. Returns true while the link is lost.
bool rack_main_tick(rack_ctx_t *ctx);

// One pass of the loop: the time, the serial bytes, the tick.
bool rack_main_step(rack_ctx_t *ctx, uint32_t now_ms, rack_reply_fn reply, void *user);

#endif // RACK_MAIN_LOOP_H
