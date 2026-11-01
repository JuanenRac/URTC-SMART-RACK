// =============================================================================
// URTC-SMART-RACK Firmware - The frames between the rack and the head in its slot: head_link.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "head_link.h"

#include <stddef.h>
#include <string.h>

head_link_config_t head_link_config_default(void)
{
    head_link_config_t config;
    config.setpoint_refresh_ms = HEAD_LINK_DEFAULT_REFRESH_MS;
    config.hotair_blower_duty = HEAD_LINK_DEFAULT_HOTAIR_BLOWER_DUTY;
    config.counters_period_ms = HEAD_LINK_DEFAULT_COUNTERS_PERIOD_MS;
    return config;
}

static bool empty_query(uint32_t id, head_frame_t *out)
{
    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->id = id;
    out->dlc = 0u;
    return true;
}

bool head_link_build_query_active_tool(head_frame_t *out)
{
    return empty_query(HEAD_LINK_ID_QUERY_ACTIVE_TOOL, out);
}

bool head_link_build_query_counters(head_frame_t *out)
{
    return empty_query(HEAD_LINK_ID_THERMAL_STATS_QUERY, out);
}

bool head_link_build_setpoint(const head_link_config_t *config, uint8_t tool_id, uint16_t target_c, head_frame_t *out)
{
    if (config == NULL || out == NULL) {
        return false;
    }
    if (tool_id == URTC_TOOL_ID_SOLDERING_IRON) {
        memset(out, 0, sizeof(*out));
        out->id = HEAD_LINK_ID_SOLDER_SETPOINT;
        out->dlc = 2u;
    } else if (tool_id == URTC_TOOL_ID_HOTAIR_REWORK) {
        memset(out, 0, sizeof(*out));
        out->id = HEAD_LINK_ID_HOTAIR_CMD;
        out->dlc = 3u;
        out->data[2] = config->hotair_blower_duty;
    } else {
        return false;
    }
    out->data[0] = (uint8_t)(target_c >> 8);
    out->data[1] = (uint8_t)(target_c & 0xFFu);
    return true;
}

bool head_link_parse_active_tool(uint32_t id, const uint8_t *data, uint8_t dlc, head_state_t *out)
{
    if (id != HEAD_LINK_ID_ACTIVE_TOOL_RESP || data == NULL || out == NULL || dlc < 4u) {
        return false;
    }
    out->tool_id = data[0];
    out->system_error = data[1] != 0u;
    out->can_error = data[2] != 0u;
    out->booting = data[3] != 0u;
    out->thermal_fault = dlc >= 5u ? data[4] : 0xFFu;
    out->reset_cause = dlc >= 6u ? data[5] : 0xFFu;
    return true;
}

bool head_link_parse_counters(uint32_t id, const uint8_t *data, uint8_t dlc, head_counters_t *out)
{
    if (id != HEAD_LINK_ID_THERMAL_STATS_RESP || data == NULL || out == NULL || dlc != 8u) {
        return false;
    }
    out->fault_count = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    out->heater_on_seconds = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 8) | data[5];
    out->last_fault = data[6];
    out->persisted = data[7] != 0u;
    return true;
}

bool head_link_setpoint_due(const head_link_config_t *config, head_setpoint_state_t *state, uint16_t target_c, uint32_t now_ms)
{
    if (config == NULL || state == NULL) {
        return false;
    }
    const bool due = !state->sent_once || state->last_target_c != target_c || (uint32_t)(now_ms - state->last_sent_ms) >= config->setpoint_refresh_ms;
    if (due) {
        state->sent_once = true;
        state->last_target_c = target_c;
        state->last_sent_ms = now_ms;
    }
    return due;
}

void head_link_setpoint_reset(head_setpoint_state_t *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}
