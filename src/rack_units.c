// =============================================================================
// URTC-SMART-RACK Firmware - Identity and wear of each physical tool head: rack_units.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "rack_units.h"

#include <stddef.h>

#include "tool_id.h"

static uint32_t add_saturating(uint32_t a, uint32_t b)
{
    return (b > UINT32_MAX - a) ? UINT32_MAX : a + b;
}

void rack_units_init(rack_units_t *u)
{
    if (u == NULL) {
        return;
    }
    for (uint8_t i = 0; i < RACK_UNITS_MAX; i++) {
        rack_unit_t *r = &u->units[i];
        r->used = false;
        r->tool_id = TOOL_ID_NONE;
        r->serial = 0u;
        lifecycle_init(&r->use);
        r->heater_seconds = 0u;
        r->faults = 0u;
        r->last_fault = 0u;
        r->counters_seen = false;
        r->seen_heater_seconds = 0u;
        r->seen_faults = 0u;
    }
}

rack_unit_t *rack_units_find(rack_units_t *u, uint8_t tool_id, uint8_t serial)
{
    if (u == NULL || serial == 0u || !tool_id_is_valid_urtc(tool_id)) {
        return NULL;
    }
    for (uint8_t i = 0; i < RACK_UNITS_MAX; i++) {
        if (u->units[i].used && u->units[i].tool_id == tool_id && u->units[i].serial == serial) {
            return &u->units[i];
        }
    }
    return NULL;
}

rack_unit_t *rack_units_get(rack_units_t *u, uint8_t tool_id, uint8_t serial)
{
    rack_unit_t *found = rack_units_find(u, tool_id, serial);
    if (found != NULL || u == NULL || serial == 0u || !tool_id_is_valid_urtc(tool_id)) {
        return found;
    }
    for (uint8_t i = 0; i < RACK_UNITS_MAX; i++) {
        if (!u->units[i].used) {
            rack_unit_t *r = &u->units[i];
            r->used = true;
            r->tool_id = tool_id;
            r->serial = serial;
            return r;
        }
    }
    return NULL;
}

bool rack_units_record_use(rack_units_t *u, uint8_t tool_id, uint8_t serial, uint32_t duration_seconds)
{
    rack_unit_t *r = rack_units_get(u, tool_id, serial);
    if (r == NULL) {
        return false;
    }
    lifecycle_record_use(&r->use, duration_seconds);
    return true;
}

bool rack_units_note_counters(rack_units_t *u, uint8_t tool_id, uint8_t serial, uint16_t fault_count, uint32_t heater_on_seconds,
                              uint8_t last_fault, bool persisted)
{
    (void)persisted;
    rack_unit_t *r = rack_units_get(u, tool_id, serial);
    if (r == NULL) {
        return false;
    }
    if (r->counters_seen) {
        if (heater_on_seconds >= r->seen_heater_seconds) {
            r->heater_seconds = add_saturating(r->heater_seconds, heater_on_seconds - r->seen_heater_seconds);
        }
        if (fault_count >= r->seen_faults) {
            r->faults = add_saturating(r->faults, (uint32_t)(fault_count - r->seen_faults));
        }
    } else {
        // first reading of this unit: what the head already counted is its history, and it belongs to the unit
        r->heater_seconds = add_saturating(r->heater_seconds, heater_on_seconds);
        r->faults = add_saturating(r->faults, fault_count);
        r->counters_seen = true;
    }
    r->seen_heater_seconds = heater_on_seconds;
    r->seen_faults = fault_count;
    if (fault_count > 0u) {
        r->last_fault = last_fault;
    }
    return true;
}

bool rack_units_needs_maintenance(rack_units_t *u, uint8_t tool_id, uint8_t serial, const rack_maintenance_limits_t *limits)
{
    rack_unit_t *r = rack_units_find(u, tool_id, serial);
    if (r == NULL || limits == NULL) {
        return false;
    }
    if (limits->max_cycles != 0u && lifecycle_needs_maintenance(&r->use, limits->max_cycles)) {
        return true;
    }
    if (limits->max_heater_seconds != 0u && r->heater_seconds >= limits->max_heater_seconds) {
        return true;
    }
    return limits->max_faults != 0u && r->faults >= limits->max_faults;
}

uint8_t rack_units_count(const rack_units_t *u)
{
    uint8_t n = 0u;
    if (u == NULL) {
        return 0u;
    }
    for (uint8_t i = 0; i < RACK_UNITS_MAX; i++) {
        if (u->units[i].used) {
            n++;
        }
    }
    return n;
}
