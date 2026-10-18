// =============================================================================
// URTC-SMART-RACK Firmware - Multi-slot tool inventory: rack_inventory.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "rack_inventory.h"
#include "tool_id.h"
#include "tool_limits.h"

#include <stddef.h>

void rack_inventory_init(rack_inventory_t *inv)
{
    if (inv == NULL) {
        return;
    }
    // Explicit field writes instead of memset(): the firmware image links without a C library.
    for (uint8_t i = 0; i < RACK_INVENTORY_MAX_SLOTS; i++) {
        inv->slots[i].occupied = false;
        inv->slots[i].tool_id = TOOL_ID_NONE;
        inv->slots[i].target_temp_c = 0;
    }
}

bool rack_inventory_set_slot_tool(rack_inventory_t *inv, uint8_t slot, uint8_t tool_id)
{
    if (inv == NULL || slot >= RACK_INVENTORY_MAX_SLOTS) {
        return false;
    }
    // Only a tool profile of the URTC catalogue (0..24) can be racked; TOOL_ID_NONE clears the slot.
    if (tool_id_is_present(tool_id) && !tool_id_is_valid_urtc(tool_id)) {
        return false;
    }

    if (!tool_id_is_present(tool_id)) {
        inv->slots[slot].occupied = false;
        inv->slots[slot].tool_id = TOOL_ID_NONE;
        inv->slots[slot].target_temp_c = 0;
        return true;
    }

    // A real physical tool can never be racked in two slots at once - if
    // this exact tool_id is already sitting in a DIFFERENT slot, it just
    // physically moved (or the same swap is being reported twice); clear
    // its old slot rather than letting the inventory claim it's in both
    // places simultaneously.
    for (uint8_t i = 0; i < RACK_INVENTORY_MAX_SLOTS; i++) {
        if (i != slot && inv->slots[i].occupied && inv->slots[i].tool_id == tool_id) {
            inv->slots[i].occupied = false;
            inv->slots[i].tool_id = TOOL_ID_NONE;
            inv->slots[i].target_temp_c = 0;
        }
    }

    inv->slots[slot].occupied = true;
    inv->slots[slot].tool_id = tool_id;
    // A tool freshly racked (or re-racked into a new slot) has no
    // pre-heat target carried over from wherever it was before - a stale
    // target must never keep heating a tool that just moved without a
    // fresh command asking for that.
    inv->slots[slot].target_temp_c = 0;
    return true;
}

bool rack_inventory_get_slot(const rack_inventory_t *inv, uint8_t slot, rack_slot_t *out_slot)
{
    if (inv == NULL || out_slot == NULL || slot >= RACK_INVENTORY_MAX_SLOTS) {
        return false;
    }
    *out_slot = inv->slots[slot];
    return true;
}

bool rack_inventory_find_tool(const rack_inventory_t *inv, uint8_t tool_id, uint8_t *out_slot)
{
    if (inv == NULL || out_slot == NULL || !tool_id_is_present(tool_id)) {
        return false;
    }
    for (uint8_t i = 0; i < RACK_INVENTORY_MAX_SLOTS; i++) {
        if (inv->slots[i].occupied && inv->slots[i].tool_id == tool_id) {
            *out_slot = i;
            return true;
        }
    }
    return false;
}

bool rack_inventory_set_preheat_target(rack_inventory_t *inv, uint8_t tool_id, uint16_t target_temp_c)
{
    uint8_t slot;
    if (!rack_inventory_find_tool(inv, tool_id, &slot)) {
        return false;
    }
    if (target_temp_c > tool_limits_max_preheat_c(tool_id)) {
        return false; // above what this tool accepts (0 when it has no heater)
    }
    inv->slots[slot].target_temp_c = target_temp_c;
    return true;
}

uint8_t rack_inventory_occupied_count(const rack_inventory_t *inv)
{
    uint8_t count = 0;
    if (inv == NULL) {
        return 0;
    }
    for (uint8_t i = 0; i < RACK_INVENTORY_MAX_SLOTS; i++) {
        if (inv->slots[i].occupied) {
            count++;
        }
    }
    return count;
}

rack_reconcile_report_t rack_inventory_reconcile(rack_inventory_t *inv, uint8_t presence_mask)
{
    rack_reconcile_report_t report = {0u, 0u};
    if (inv == NULL) {
        return report;
    }
    for (uint8_t i = 0; i < RACK_INVENTORY_MAX_SLOTS; i++) {
        const uint8_t bit = (uint8_t)(1u << i);
        const bool sensed = (presence_mask & bit) != 0u;
        if (inv->slots[i].occupied && !sensed) {
            inv->slots[i].occupied = false;
            inv->slots[i].tool_id = TOOL_ID_NONE;
            inv->slots[i].target_temp_c = 0;
            report.removed_mask |= bit;
        } else if (!inv->slots[i].occupied && sensed) {
            report.unidentified_mask |= bit;
        }
    }
    return report;
}
