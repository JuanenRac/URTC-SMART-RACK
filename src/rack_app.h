// =============================================================================
// URTC-SMART-RACK Firmware - Receive path to inventory: rack_app.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// Ties the decision of rack_link_decide() to the state it is about: the
// multi-slot inventory. Until now the inventory was not connected to anything;
// this module is the one place that turns "tool 0 -> 200 C", "tool 7 -> safe
// state" and "the link is lost" into pre-heat targets per slot, so the main
// loop only has to call two functions:
//
//   rack_link_decide(&lw, frame, len, now, &decision);   // when a frame arrives
//   rack_app_apply_decision(&inventory, &decision);
//   ...
//   rack_app_tick(&inventory, &lw, now);                  // every pass of the main loop
//
// Pure logic: the targets stay numbers in the inventory until a heater driver
// exists to act on them.
#ifndef RACK_APP_H
#define RACK_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "link_watchdog.h"
#include "preheat_limit.h"
#include "rack_inventory.h"
#include "rack_link.h"

// Applies one decision. SET_TARGET stores the target for the slot that holds the
// tool; SAFE_STATE stores the safe temperature (off) for it. A decision about a
// tool that is not racked, or no action at all, changes nothing. Returns true
// when a target was written.
bool rack_app_apply_decision(rack_inventory_t *inv, const rack_decision_t *decision);

// Brings every slot to the safe state: all pre-heat targets go to the safe
// temperature. Returns the number of slots whose target changed.
uint8_t rack_app_enter_safe_state(rack_inventory_t *inv);

// One pass of the main loop: lets the watchdog latch a link loss and, while the
// link is lost (or was never proven alive), keeps every slot in the safe state.
// Returns true while the link is lost.
bool rack_app_tick(rack_inventory_t *inv, link_watchdog_t *lw, uint32_t now_ms);

// The same two calls with the longest pre-heat without a swap (preheat_limit.h): a target above 0 is refused while its slot is latched after
// running out its time, and the tick switches a slot off when its limit is reached even though the link is alive. `limit` may be NULL (no limit,
// which is what the two functions above do).
bool rack_app_apply_decision_limited(rack_inventory_t *inv, preheat_limit_t *limit, const rack_decision_t *decision, uint32_t now_ms);
bool rack_app_tick_limited(rack_inventory_t *inv, link_watchdog_t *lw, preheat_limit_t *limit, uint32_t now_ms);

#endif // RACK_APP_H
