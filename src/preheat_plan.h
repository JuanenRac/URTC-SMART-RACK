// =============================================================================
// URTC-SMART-RACK Firmware - Predictive pre-heat from the job queue: preheat_plan.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// preheat_should_activate() needs to know how long until a tool is next used. That time comes from the job queue of the
// orchestrator (the list of the next jobs, each with the tool it needs and how long until it starts). This turns the queue into
// orders for the rack: for every racked tool that has a heater and a job whose start is nearer than the time the tool needs to
// heat up (plus a margin), a pre-heat order with the default target of the tool; optionally, an order to switch off a tool that is
// hot and has no job soon.
//
// Switched OFF by default (`enabled` false gives no order at all). The orders go through the same path as a command from a host
// (rack_command, the link watchdog, the 10-minute limit of preheat_limit): this module only suggests them, it never touches a
// heater. The heating times are estimates for a bench to confirm.
#ifndef PREHEAT_PLAN_H
#define PREHEAT_PLAN_H

#include <stdbool.h>
#include <stdint.h>

#include "rack_inventory.h"

#define PREHEAT_PLAN_MAX_ORDERS RACK_INVENTORY_MAX_SLOTS

typedef struct {
    uint8_t tool_id;
    uint32_t starts_in_ms;   // 0 = the job is already due
} preheat_job_t;

typedef struct {
    bool enabled;            // false: no order at all (the default of preheat_plan_config_default())
    uint8_t margin_percent;  // extra time on top of the estimated heating time
    bool cool_unused;        // also order target 0 for a hot tool that no job needs soon
    uint32_t hold_ms;        // a hot tool whose next job is further away than lead + hold is switched off (when cool_unused)
} preheat_plan_config_t;

typedef struct {
    uint8_t tool_id;
    uint16_t target_c;       // 0 = switch off
} preheat_order_t;

preheat_plan_config_t preheat_plan_config_default(void);

// The default pre-heat target of a tool (0 when it has no heater the rack warms up).
uint16_t preheat_plan_default_target_c(uint8_t tool_id);

// How long a tool needs to reach `target_c` from room temperature, with the margin; 0 for a tool without heater.
uint32_t preheat_plan_lead_ms(uint8_t tool_id, uint16_t target_c, uint8_t margin_percent);

// Fills `orders` (room for PREHEAT_PLAN_MAX_ORDERS) and returns how many. A job for a tool that is not racked, or whose job is already
// due but whose pre-heat is already at its target, gives no order. Only the earliest job of each tool counts.
uint8_t preheat_plan_decide(const preheat_plan_config_t *config, const rack_inventory_t *inv, const preheat_job_t *jobs, uint8_t job_count,
                            preheat_order_t orders[PREHEAT_PLAN_MAX_ORDERS]);

#endif // PREHEAT_PLAN_H
