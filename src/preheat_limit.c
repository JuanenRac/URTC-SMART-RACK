// =============================================================================
// URTC-SMART-RACK Firmware - Longest pre-heat without a tool swap: preheat_limit.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "preheat_limit.h"

#include <stddef.h>

#include "preheat.h"

static void clear_slot(preheat_limit_t *pl, uint8_t slot)
{
    pl->running[slot] = false;
    pl->expired[slot] = false;
    pl->started_ms[slot] = 0;
    pl->tool_id[slot] = 0;
}

void preheat_limit_init(preheat_limit_t *pl, uint32_t max_ms)
{
    if (pl == NULL) {
        return;
    }
    pl->max_ms = (max_ms == 0u) ? RACK_PREHEAT_MAX_MS : max_ms;
    pl->rearm_ms = RACK_PREHEAT_REARM_MS;
    for (uint8_t slot = 0; slot < RACK_INVENTORY_MAX_SLOTS; slot++) {
        clear_slot(pl, slot);
        pl->cooling[slot] = false;
        pl->off_ms[slot] = 0;
        pl->off_tool[slot] = 0;
    }
}

void preheat_limit_set_rearm(preheat_limit_t *pl, uint32_t rearm_ms)
{
    if (pl != NULL) {
        pl->rearm_ms = rearm_ms;
    }
}

bool preheat_limit_allow(preheat_limit_t *pl, uint8_t slot, uint8_t tool_id, uint16_t target_temp_c, uint32_t now_ms)
{
    if (pl == NULL || slot >= RACK_INVENTORY_MAX_SLOTS) {
        return false;
    }
    if (target_temp_c == 0u) {
        if (pl->rearm_ms != 0u && (pl->running[slot] || pl->expired[slot])) {
            // it was heating: remember when and for which tool, so the next pre-heat waits for the cooling time
            pl->off_tool[slot] = pl->tool_id[slot];
            pl->off_ms[slot] = now_ms;
            pl->cooling[slot] = true;
        }
        clear_slot(pl, slot); // the host switched it off: the pre-heat is over and so is any latch
        return true;
    }
    if (pl->max_ms == PREHEAT_LIMIT_OFF) {
        return true;
    }
    if (pl->cooling[slot]) {
        // Unsigned subtraction: correct across the wrap of the millisecond counter.
        if (pl->off_tool[slot] != tool_id || (uint32_t)(now_ms - pl->off_ms[slot]) >= pl->rearm_ms) {
            pl->cooling[slot] = false; // another tool, or it has cooled long enough
        } else {
            return false; // the same tool was switched off a moment ago: it waits
        }
    }
    if ((pl->running[slot] || pl->expired[slot]) && pl->tool_id[slot] != tool_id) {
        clear_slot(pl, slot); // another tool in the slot is a new pre-heat, not the one that ran out
    }
    if (pl->expired[slot]) {
        return false;
    }
    if (!pl->running[slot]) {
        pl->running[slot] = true;
        pl->started_ms[slot] = now_ms;
        pl->tool_id[slot] = tool_id;
    }
    return true;
}

uint8_t preheat_limit_tick(preheat_limit_t *pl, rack_inventory_t *inv, bool link_lost, uint32_t now_ms)
{
    if (pl == NULL || inv == NULL) {
        return 0;
    }
    uint8_t switched_off = 0;
    for (uint8_t slot = 0; slot < RACK_INVENTORY_MAX_SLOTS; slot++) {
        const rack_slot_t *entry = &inv->slots[slot];
        const bool holds_the_tool = entry->occupied && entry->tool_id == pl->tool_id[slot];
        if (pl->expired[slot]) {
            if (!holds_the_tool) {
                clear_slot(pl, slot); // swapped or pulled: the latch is over
            }
            continue;
        }
        if (!pl->running[slot]) {
            continue;
        }
        if (link_lost || !holds_the_tool || entry->target_temp_c == preheat_safe_state_temp_c()) {
            clear_slot(pl, slot);
            continue;
        }
        // Unsigned subtraction: correct across the wrap of the millisecond counter.
        if ((uint32_t)(now_ms - pl->started_ms[slot]) >= pl->max_ms) {
            inv->slots[slot].target_temp_c = preheat_safe_state_temp_c();
            pl->running[slot] = false;
            pl->expired[slot] = true;
            switched_off++;
        }
    }
    return switched_off;
}

bool preheat_limit_is_expired(const preheat_limit_t *pl, uint8_t slot)
{
    if (pl == NULL || slot >= RACK_INVENTORY_MAX_SLOTS) {
        return false;
    }
    return pl->expired[slot];
}
