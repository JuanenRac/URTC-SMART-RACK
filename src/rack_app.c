// =============================================================================
// URTC-SMART-RACK Firmware - Receive path to inventory: rack_app.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "rack_app.h"

#include <stddef.h>

#include "preheat.h"
#include "preheat_limit.h"

bool rack_app_apply_decision_limited(rack_inventory_t *inv, preheat_limit_t *limit, const rack_decision_t *decision, uint32_t now_ms)
{
    if (inv == NULL || decision == NULL) {
        return false;
    }
    uint16_t target;
    switch (decision->action) {
        case RACK_ACT_SET_TARGET:
            target = decision->temp_c;
            break;
        case RACK_ACT_SAFE_STATE:
            target = preheat_safe_state_temp_c();
            break;
        case RACK_ACT_NONE:
        default:
            return false;
    }
    uint8_t slot;
    if (limit != NULL && rack_inventory_find_tool(inv, decision->tool_id, &slot)) {
        // The limit is asked before the inventory is written, and only about a tool that is racked: a target for a tool that is not
        // in the rack is refused by the inventory anyway and must not start a clock.
        if (!preheat_limit_allow(limit, slot, decision->tool_id, target, now_ms)) {
            return false; // this slot ran out its pre-heat and waits for a 0 or a swap
        }
    }
    return rack_inventory_set_preheat_target(inv, decision->tool_id, target);
}

bool rack_app_apply_decision(rack_inventory_t *inv, const rack_decision_t *decision)
{
    return rack_app_apply_decision_limited(inv, NULL, decision, 0);
}

uint8_t rack_app_enter_safe_state(rack_inventory_t *inv)
{
    uint8_t changed = 0;
    if (inv == NULL) {
        return 0;
    }
    for (uint8_t i = 0; i < RACK_INVENTORY_MAX_SLOTS; i++) {
        if (inv->slots[i].target_temp_c != preheat_safe_state_temp_c()) {
            inv->slots[i].target_temp_c = preheat_safe_state_temp_c();
            changed++;
        }
    }
    return changed;
}

bool rack_app_tick_limited(rack_inventory_t *inv, link_watchdog_t *lw, preheat_limit_t *limit, uint32_t now_ms)
{
    link_watchdog_tick(lw, now_ms);
    const bool lost = link_watchdog_is_link_lost(lw, now_ms);
    if (lost) {
        rack_app_enter_safe_state(inv);
    }
    if (limit != NULL) {
        preheat_limit_tick(limit, inv, lost, now_ms);
    }
    return lost;
}

bool rack_app_tick(rack_inventory_t *inv, link_watchdog_t *lw, uint32_t now_ms)
{
    return rack_app_tick_limited(inv, lw, NULL, now_ms);
}
