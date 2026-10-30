// =============================================================================
// URTC-SMART-RACK Firmware - Predictive pre-heat from the job queue: preheat_plan.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "preheat_plan.h"

#include <stddef.h>

#include "preheat.h"
#include "tool_id.h"
#include "tool_limits.h"

#define ROOM_TEMP_C 25u

preheat_plan_config_t preheat_plan_config_default(void)
{
    preheat_plan_config_t c;
    c.enabled = false;
    c.margin_percent = 50u;
    c.cool_unused = false;
    c.hold_ms = 120000u;
    c.printer_target_c = 0u; // off: the temperature of a hotend depends on the filament, so it is the person who sets it
    return c;
}

static tool_type_t type_of(uint8_t tool_id)
{
    if (tool_id == TOOL_ID_SOLDERING_IRON) {
        return TOOL_TYPE_SOLDERING_IRON;
    }
    if (tool_id == TOOL_ID_HOTAIR_REWORK) {
        return TOOL_TYPE_HOT_AIR;
    }
    return TOOL_TYPE_GENERIC;
}

uint16_t preheat_plan_default_target_c(uint8_t tool_id)
{
    const uint16_t wanted = preheat_target_temp_c(type_of(tool_id));
    const uint16_t ceiling = tool_limits_max_preheat_c(tool_id);
    return wanted > ceiling ? ceiling : wanted;
}

uint16_t preheat_plan_target_c(const preheat_plan_config_t *config, uint8_t tool_id)
{
    if (tool_id == TOOL_ID_3D_PRINTER) {
        const uint16_t ceiling = tool_limits_max_preheat_c(tool_id);
        const uint16_t wanted = config != NULL ? config->printer_target_c : 0u;
        return wanted > ceiling ? ceiling : wanted;
    }
    return preheat_plan_default_target_c(tool_id);
}

// Milliseconds to climb 100 degrees, by tool: estimates, to be confirmed on a bench.
static uint32_t ms_per_100c(uint8_t tool_id)
{
    switch (tool_id) {
        case TOOL_ID_SOLDERING_IRON: return 10000u;
        case TOOL_ID_HOTAIR_REWORK: return 15000u;
        case TOOL_ID_3D_PRINTER: return 20000u;
        default: return 0u;
    }
}

uint32_t preheat_plan_lead_ms(uint8_t tool_id, uint16_t target_c, uint8_t margin_percent)
{
    const uint32_t per100 = ms_per_100c(tool_id);
    if (per100 == 0u || target_c <= ROOM_TEMP_C) {
        return 0u;
    }
    const uint32_t climb = (uint32_t)target_c - ROOM_TEMP_C;
    const uint32_t base = climb * per100 / 100u;
    return base + base * margin_percent / 100u;
}

uint8_t preheat_plan_decide(const preheat_plan_config_t *config, const rack_inventory_t *inv, const preheat_job_t *jobs, uint8_t job_count,
                            preheat_order_t orders[PREHEAT_PLAN_MAX_ORDERS])
{
    uint8_t n = 0u;
    if (config == NULL || !config->enabled || inv == NULL || orders == NULL || (jobs == NULL && job_count > 0u)) {
        return 0u;
    }
    for (uint8_t slot = 0; slot < RACK_INVENTORY_MAX_SLOTS && n < PREHEAT_PLAN_MAX_ORDERS; slot++) {
        const rack_slot_t *s = &inv->slots[slot];
        if (!s->occupied) {
            continue;
        }
        const uint16_t target = preheat_plan_target_c(config, s->tool_id);
        if (target == 0u) {
            continue; // no heater the rack warms up
        }
        bool has_job = false;
        uint32_t next_ms = 0u;
        for (uint8_t j = 0; j < job_count; j++) {
            if (jobs[j].tool_id == s->tool_id && (!has_job || jobs[j].starts_in_ms < next_ms)) {
                has_job = true;
                next_ms = jobs[j].starts_in_ms;
            }
        }
        const uint32_t lead = preheat_plan_lead_ms(s->tool_id, target, config->margin_percent);
        if (has_job && (next_ms == 0u || preheat_should_activate(next_ms, lead))) {
            if (s->target_temp_c != target) {
                orders[n].tool_id = s->tool_id;
                orders[n].target_c = target;
                n++;
            }
        } else if (config->cool_unused && s->target_temp_c != 0u && (!has_job || next_ms > lead + config->hold_ms)) {
            orders[n].tool_id = s->tool_id;
            orders[n].target_c = 0u;
            n++;
        }
    }
    return n;
}
