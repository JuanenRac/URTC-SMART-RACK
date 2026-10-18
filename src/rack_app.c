// =============================================================================
// URTC-SMART-RACK Firmware - Receive path to inventory: rack_app.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "rack_app.h"

#include <stddef.h>

#include "preheat.h"

bool rack_app_apply_decision(rack_inventory_t *inv, const rack_decision_t *decision)
{
    if (inv == NULL || decision == NULL) {
        return false;
    }
    switch (decision->action) {
        case RACK_ACT_SET_TARGET:
            return rack_inventory_set_preheat_target(inv, decision->tool_id, decision->temp_c);
        case RACK_ACT_SAFE_STATE:
            return rack_inventory_set_preheat_target(inv, decision->tool_id, preheat_safe_state_temp_c());
        case RACK_ACT_NONE:
        default:
            return false;
    }
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

bool rack_app_tick(rack_inventory_t *inv, link_watchdog_t *lw, uint32_t now_ms)
{
    link_watchdog_tick(lw, now_ms);
    const bool lost = link_watchdog_is_link_lost(lw, now_ms);
    if (lost) {
        rack_app_enter_safe_state(inv);
    }
    return lost;
}
