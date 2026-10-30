// =============================================================================
// URTC-SMART-RACK Firmware - Free-configuration heads (ID jumpers 0x1F): free_tool.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "free_tool.h"

#include <stddef.h>

#include "tool_id.h"

free_tool_config_t free_tool_config_default(void)
{
    free_tool_config_t c;
    c.query_timeout_ms = FREE_TOOL_DEFAULT_QUERY_TIMEOUT_MS;
    c.max_attempts = (uint8_t)FREE_TOOL_DEFAULT_MAX_ATTEMPTS;
    c.retry_backoff_ms = FREE_TOOL_DEFAULT_RETRY_BACKOFF_MS;
    c.refresh_period_ms = FREE_TOOL_DEFAULT_REFRESH_PERIOD_MS;
    return c;
}

free_tool_kind_t free_tool_classify_jumpers(uint8_t raw_bits, uint8_t *tool_out)
{
    const uint8_t id = tool_id_decode(raw_bits);
    if (tool_id_is_valid_urtc(id)) {
        if (tool_out != NULL) {
            *tool_out = id;
        }
        return FREE_TOOL_KIND_DIRECT;
    }
    return id == TOOL_ID_FREE_CONFIG ? FREE_TOOL_KIND_FREE : FREE_TOOL_KIND_INVALID;
}

bool free_tool_parse_reply(const uint8_t *data, uint8_t len, uint8_t *tool_out)
{
    if (data == NULL || len < FREE_TOOL_REPLY_MIN_LEN || data[0] != FREE_TOOL_JUMPER_READING) {
        return false;
    }
    const uint8_t selection = data[1];
    if (selection < 1u || selection > FREE_TOOL_SELECTION_MAX) {
        return false;
    }
    if (tool_out != NULL) {
        *tool_out = (uint8_t)(selection - 1u);
    }
    return true;
}

void free_tool_reset(free_tool_t *ft)
{
    if (ft == NULL) {
        return;
    }
    ft->resolved = false;
    ft->tool = TOOL_ID_NONE;
    ft->attempts = 0u;
    ft->armed = false;
    ft->deadline_ms = 0u;
    ft->next_ms = 0u;
}

// True when `now` has reached `when`, across the wrap of the millisecond counter.
static bool due(uint32_t now_ms, uint32_t when_ms)
{
    return (uint32_t)(now_ms - when_ms) < 0x80000000u;
}

static void settle(free_tool_t *ft, const free_tool_config_t *config, uint32_t now_ms, bool backoff)
{
    ft->attempts = 0u;
    ft->armed = true;
    ft->next_ms = now_ms + (backoff ? config->retry_backoff_ms : config->refresh_period_ms);
}

static free_tool_event_t on_reply(free_tool_t *ft, const free_tool_config_t *config, uint32_t now_ms, const uint8_t *reply, uint8_t len)
{
    uint8_t tool = TOOL_ID_NONE;
    if (!free_tool_parse_reply(reply, len, &tool)) {
        const bool was = ft->resolved;
        ft->resolved = false;
        ft->tool = TOOL_ID_NONE;
        settle(ft, config, now_ms, true);
        return was ? FREE_TOOL_EVT_LOST : FREE_TOOL_EVT_NONE;
    }
    const bool was = ft->resolved;
    const uint8_t previous = ft->tool;
    ft->resolved = true;
    ft->tool = tool;
    settle(ft, config, now_ms, false);
    if (!was) {
        return FREE_TOOL_EVT_RESOLVED;
    }
    return previous == tool ? FREE_TOOL_EVT_CONFIRMED : FREE_TOOL_EVT_CHANGED;
}

static bool poll(free_tool_t *ft, const free_tool_config_t *config, uint32_t now_ms, free_tool_event_t *event)
{
    *event = FREE_TOOL_EVT_NONE;
    if (ft->attempts == 0u) {
        if (ft->armed && !due(now_ms, ft->next_ms)) {
            return false;
        }
        if (ft->armed && ft->resolved && config->refresh_period_ms == 0u) {
            return false;
        }
        ft->armed = true;
        ft->attempts = 1u;
        ft->deadline_ms = now_ms + config->query_timeout_ms;
        return true;
    }
    if (!due(now_ms, ft->deadline_ms)) {
        return false;
    }
    const uint8_t limit = config->max_attempts == 0u ? 1u : config->max_attempts;
    if (ft->attempts < limit) {
        ft->attempts++;
        ft->deadline_ms = now_ms + config->query_timeout_ms;
        return true;
    }
    const bool was = ft->resolved;
    ft->resolved = false;
    ft->tool = TOOL_ID_NONE;
    settle(ft, config, now_ms, true);
    *event = was ? FREE_TOOL_EVT_LOST : FREE_TOOL_EVT_NONE;
    return false;
}

bool free_tool_step(free_tool_t *ft, const free_tool_config_t *config, uint32_t now_ms, const uint8_t *reply, uint8_t reply_len,
                    free_tool_event_t *event)
{
    free_tool_event_t from_reply = FREE_TOOL_EVT_NONE;
    free_tool_event_t from_clock = FREE_TOOL_EVT_NONE;
    if (ft == NULL || config == NULL) {
        if (event != NULL) {
            *event = FREE_TOOL_EVT_NONE;
        }
        return false;
    }
    if (reply != NULL) {
        from_reply = on_reply(ft, config, now_ms, reply, reply_len);
    }
    const bool query = poll(ft, config, now_ms, &from_clock);
    if (event != NULL) {
        *event = from_reply != FREE_TOOL_EVT_NONE ? from_reply : from_clock;
    }
    return query;
}

void free_tool_bank_init(free_tool_bank_t *bank)
{
    if (bank == NULL) {
        return;
    }
    for (uint8_t i = 0; i < RACK_INVENTORY_MAX_SLOTS; i++) {
        free_tool_reset(&bank->head[i]);
        bank->free_mode[i] = false;
        bank->invalid[i] = false;
    }
}

free_tool_kind_t free_tool_bank_set_jumpers(free_tool_bank_t *bank, rack_inventory_t *inv, uint8_t slot, uint8_t raw_bits)
{
    if (bank == NULL || inv == NULL || slot >= RACK_INVENTORY_MAX_SLOTS) {
        return FREE_TOOL_KIND_INVALID;
    }
    uint8_t tool = TOOL_ID_NONE;
    const free_tool_kind_t kind = free_tool_classify_jumpers(raw_bits, &tool);
    free_tool_reset(&bank->head[slot]);
    bank->free_mode[slot] = (kind == FREE_TOOL_KIND_FREE);
    bank->invalid[slot] = (kind == FREE_TOOL_KIND_INVALID);
    if (kind == FREE_TOOL_KIND_DIRECT) {
        (void)rack_inventory_set_slot_unit(inv, slot, tool, 0u);
    } else {
        (void)rack_inventory_set_slot_unit(inv, slot, TOOL_ID_NONE, 0u);
    }
    return kind;
}

bool free_tool_bank_step(free_tool_bank_t *bank, const free_tool_config_t *config, rack_inventory_t *inv, uint8_t slot, uint32_t now_ms,
                         const uint8_t *reply, uint8_t reply_len, free_tool_event_t *event)
{
    if (event != NULL) {
        *event = FREE_TOOL_EVT_NONE;
    }
    if (bank == NULL || inv == NULL || slot >= RACK_INVENTORY_MAX_SLOTS || !bank->free_mode[slot]) {
        return false;
    }
    free_tool_event_t e = FREE_TOOL_EVT_NONE;
    const bool query = free_tool_step(&bank->head[slot], config, now_ms, reply, reply_len, &e);
    if (e == FREE_TOOL_EVT_RESOLVED || e == FREE_TOOL_EVT_CHANGED) {
        // A change of tool: the slot takes the new tool with no target (rack_inventory_set_slot_unit clears it).
        (void)rack_inventory_set_slot_unit(inv, slot, bank->head[slot].tool, 0u);
    } else if (e == FREE_TOOL_EVT_LOST) {
        (void)rack_inventory_set_slot_unit(inv, slot, TOOL_ID_NONE, 0u);
    }
    if (event != NULL) {
        *event = e;
    }
    return query;
}

free_tool_identity_t free_tool_bank_identity(const free_tool_bank_t *bank, const rack_inventory_t *inv, uint8_t slot)
{
    if (bank == NULL || inv == NULL || slot >= RACK_INVENTORY_MAX_SLOTS) {
        return FREE_TOOL_ID_EMPTY;
    }
    if (inv->slots[slot].occupied) {
        return FREE_TOOL_ID_TOOL;
    }
    if (bank->free_mode[slot]) {
        return FREE_TOOL_ID_PENDING;
    }
    return bank->invalid[slot] ? FREE_TOOL_ID_INVALID : FREE_TOOL_ID_EMPTY;
}
