// =============================================================================
// URTC-SMART-RACK Firmware - Identity and wear of each physical tool head: rack_units.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The five identity bits of a head say its TYPE, not WHICH head it is. A head also holds a serial number in its F-RAM (CAN 0x1A4 /
// 0x1A5), and the lifetime thermal counters of 0x1AA / 0x1AB. The rack keeps one record per unit, keyed by the type plus the serial
// number, so that wear follows the physical head from slot to slot and from rack to rack and not the type. A head whose serial is 0
// (never written) has no identity and gets no record: counting its use would credit it to a head nobody can tell apart.
#ifndef RACK_UNITS_H
#define RACK_UNITS_H

#include <stdbool.h>
#include <stdint.h>

#include "lifecycle.h"

#define RACK_UNITS_MAX 32u

typedef struct {
    bool used;
    uint8_t tool_id;
    uint8_t serial;                 // never 0 for a used record
    lifecycle_t use;                // cycles and seconds of use in the rack
    uint32_t heater_seconds;        // heater-on time accumulated from the counter of the head itself
    uint32_t faults;                // thermal faults accumulated from the counter of the head itself
    uint8_t last_fault;             // code of the last thermal fault the head reported
    bool counters_seen;             // the counters of the head have been read at least once
    uint32_t seen_heater_seconds;   // the counter of the head at the last reading
    uint16_t seen_faults;
} rack_unit_t;

typedef struct {
    rack_unit_t units[RACK_UNITS_MAX];
} rack_units_t;

typedef struct {
    uint32_t max_cycles;            // 0 = this limit is not used
    uint32_t max_heater_seconds;
    uint32_t max_faults;
} rack_maintenance_limits_t;

void rack_units_init(rack_units_t *u);

// The record of a unit, or NULL when the serial is 0, the type is not a tool profile or there is no record yet.
rack_unit_t *rack_units_find(rack_units_t *u, uint8_t tool_id, uint8_t serial);

// The record of a unit, made if it does not exist. NULL for serial 0, a type that is no tool profile, or a full table.
rack_unit_t *rack_units_get(rack_units_t *u, uint8_t tool_id, uint8_t serial);

// One use of the unit (a pick-up) of `duration_seconds`. False when the unit has no identity or there is no room.
bool rack_units_record_use(rack_units_t *u, uint8_t tool_id, uint8_t serial, uint32_t duration_seconds);

// The answer of a head to 0x1AA (0x1AB): its lifetime counters. Only the growth since the last reading is added, so reading twice does
// not count twice. A counter that went DOWN (the F-RAM of the head was erased or replaced) starts again from the new value and adds
// nothing it cannot prove. `persisted` false means the head counted in RAM only since boot; the reading is used the same way.
bool rack_units_note_counters(rack_units_t *u, uint8_t tool_id, uint8_t serial, uint16_t fault_count, uint32_t heater_on_seconds,
                              uint8_t last_fault, bool persisted);

// The wear says it is time to look at the unit. False for an unknown unit.
bool rack_units_needs_maintenance(rack_units_t *u, uint8_t tool_id, uint8_t serial, const rack_maintenance_limits_t *limits);

uint8_t rack_units_count(const rack_units_t *u);

#endif // RACK_UNITS_H
