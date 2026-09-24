// =============================================================================
// URTC-SMART-RACK Firmware - Multi-slot tool inventory: rack_inventory.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// every other module in this project already tracks one tool_id at a
// time (rack_command.c validates one command for one tool_id;
// link_watchdog.c's own per-tool-ID sequence table is about detecting a
// replayed/reordered COMMAND, not "where is this tool physically racked").
// A real Smart Rack (this project's own name, and its README's own
// "tracks... every URTC head") holds several tools at once - this module
// is the real, missing "who is racked where right now" state a multi-bay
// rack actually needs, kept as pure, host-testable logic like the rest of
// this project's pre-PCB work.
//
// RACK_INVENTORY_MAX_SLOTS is a placeholder capacity, not a claim about
// real hardware - see README's own "no PCB/schematic exists for this
// board yet". Adjust it once a real bay count is pinned down.
#ifndef RACK_INVENTORY_H
#define RACK_INVENTORY_H

#include <stdbool.h>
#include <stdint.h>

#define RACK_INVENTORY_MAX_SLOTS 8u

typedef struct {
    bool occupied;
    uint8_t tool_id;         // valid only while occupied is true
    uint16_t target_temp_c;  // this slot's last commanded pre-heat target, 0 = none/off
} rack_slot_t;

typedef struct {
    rack_slot_t slots[RACK_INVENTORY_MAX_SLOTS];
} rack_inventory_t;

// Every slot starts empty (occupied=false) - a rack that has never heard
// from a slot must never be assumed to hold anything, same "no data means
// unknown, not empty-by-luck vs really-empty" discipline this project's
// link_watchdog.c already applies to a link that has never received a frame.
void rack_inventory_init(rack_inventory_t *inv);

// Real, bounded 0..RACK_INVENTORY_MAX_SLOTS-1 write of "this tool is now
// physically racked in this slot". Passing TOOL_ID_NONE (see tool_id.h)
// clears the slot (occupied=false) instead of racking a real tool there -
// the same wire convention every other command in this project already
// uses for "no tool", so callers never need a separate remove function.
// Returns false (no-op) for an out-of-range slot.
bool rack_inventory_set_slot_tool(rack_inventory_t *inv, uint8_t slot, uint8_t tool_id);

// Real, range-checked read - an out-of-range slot returns false rather
// than reading past the array (fail closed, matching this project's own
// established convention elsewhere), and *out_slot is only written on a
// true return.
bool rack_inventory_get_slot(const rack_inventory_t *inv, uint8_t slot, rack_slot_t *out_slot);

// Is this exact tool_id ALREADY racked somewhere, and if so where? A rack
// must never let two commands believe the same physical tool sits in two
// different slots at once - this is the real check that makes that
// provable instead of assumed. Returns false (out_slot untouched) if the
// tool isn't racked anywhere right now.
bool rack_inventory_find_tool(const rack_inventory_t *inv, uint8_t tool_id, uint8_t *out_slot);

// Sets the commanded pre-heat target for whichever slot currently holds
// tool_id. Returns false (no-op) if that tool isn't racked anywhere right
// now - a pre-heat command for a tool that was already pulled must never
// silently "succeed" against a stale/absent slot.
bool rack_inventory_set_preheat_target(rack_inventory_t *inv, uint8_t tool_id, uint16_t target_temp_c);

// Count of currently-occupied slots - a real, cheap "how full is this
// rack right now" query instead of every caller looping the array itself.
uint8_t rack_inventory_occupied_count(const rack_inventory_t *inv);

#endif // RACK_INVENTORY_H
