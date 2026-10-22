// =============================================================================
// URTC-SMART-RACK Firmware - Longest pre-heat without a tool swap: preheat_limit.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// A pre-heat exists to have a tool hot when the swap happens. If the swap never comes (the
// host crashed in a state that keeps sending the same target, an operation was cancelled and
// nobody switched the heater off) a live link must not keep the heater on for ever: the
// watchdog only catches a DEAD link. This module bounds how long a slot may stay heated
// without its tool being swapped.
//
// The rules, per slot:
//   * the clock starts when the target goes from 0 to something above 0;
//   * repeating or changing the target does NOT restart it: the host cannot extend a
//     pre-heat by resending it;
//   * when the limit is reached the target goes to 0 and the slot is latched: further
//     targets above 0 are refused until the host sends a target of 0 (an explicit
//     acknowledgement) or the tool in the slot changes (the swap);
//   * a target of 0, a lost link, an emptied slot or another tool in the slot end the
//     pre-heat (and clear the latch).
//
// Configuration: the limit is a run-time value given to preheat_limit_init(). The default is
// RACK_PREHEAT_MAX_MS (10 minutes), which a build can change with -DRACK_PREHEAT_MAX_MS=...;
// PREHEAT_LIMIT_OFF disables the limit (the heater is then bounded by the link watchdog and
// by the per-tool ceilings only). Pure logic against a caller-supplied millisecond clock.
#ifndef PREHEAT_LIMIT_H
#define PREHEAT_LIMIT_H

#include <stdbool.h>
#include <stdint.h>

#include "link_watchdog.h"
#include "rack_inventory.h"

#ifndef RACK_PREHEAT_MAX_MS
#define RACK_PREHEAT_MAX_MS 600000u // 10 minutes
#endif

// Value of `max_ms` that turns the limit off.
#define PREHEAT_LIMIT_OFF 0xFFFFFFFFu

typedef struct {
    uint32_t max_ms;
    bool running[RACK_INVENTORY_MAX_SLOTS];
    bool expired[RACK_INVENTORY_MAX_SLOTS];
    uint32_t started_ms[RACK_INVENTORY_MAX_SLOTS];
    uint8_t tool_id[RACK_INVENTORY_MAX_SLOTS]; // the tool the running pre-heat belongs to
} preheat_limit_t;

// A `max_ms` of 0 is replaced by RACK_PREHEAT_MAX_MS (a zero limit would never let a heater
// start); PREHEAT_LIMIT_OFF disables the limit.
void preheat_limit_init(preheat_limit_t *pl, uint32_t max_ms);

// May the slot take `target_temp_c` now? Call it before storing a target. A target of 0 is
// always allowed and ends the pre-heat of the slot (clearing the latch). A target above 0 is
// refused (false) while the slot is latched; otherwise it is allowed and, if no pre-heat was
// running, starts the clock.
bool preheat_limit_allow(preheat_limit_t *pl, uint8_t slot, uint8_t tool_id, uint16_t target_temp_c, uint32_t now_ms);

// One pass of the main loop: ends the pre-heat of every slot whose limit is reached (the
// target goes to the safe temperature and the slot is latched), and forgets the pre-heat of a
// slot that was emptied or now holds another tool. While `link_lost` is true every running
// pre-heat ends without a latch. Returns the number of slots switched off by the limit.
uint8_t preheat_limit_tick(preheat_limit_t *pl, rack_inventory_t *inv, bool link_lost, uint32_t now_ms);

// Is the slot latched after reaching the limit?
bool preheat_limit_is_expired(const preheat_limit_t *pl, uint8_t slot);

#endif // PREHEAT_LIMIT_H
