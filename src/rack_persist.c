// =============================================================================
// URTC-SMART-RACK Firmware - Lifecycle and inventory kept across power cuts: rack_persist.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "rack_persist.h"
#include "urtc_crc8.h"
#include <stddef.h>

#define COPIES 2
#define SLOTS_AT 14u
#define SLOT_BYTES 5u
#define BODY_BYTES ((uint8_t)(RACK_PERSIST_RECORD_SIZE - 1u))

_Static_assert(SLOTS_AT + SLOT_BYTES * RACK_INVENTORY_MAX_SLOTS + 1u == RACK_PERSIST_RECORD_SIZE, "record layout");
_Static_assert(RACK_PERSIST_RECORD_SIZE <= RACK_PERSIST_COPY_STRIDE, "a copy must fit its stride");

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void encode(uint8_t *rec, uint32_t sequence, const rack_state_t *state)
{
    rec[0] = RACK_PERSIST_MAGIC;
    rec[1] = RACK_PERSIST_VERSION;
    put32(&rec[2], sequence);
    put32(&rec[6], state->lifecycle.total_cycles);
    put32(&rec[10], state->lifecycle.total_seconds);
    for (uint8_t i = 0; i < RACK_INVENTORY_MAX_SLOTS; i++) {
        const rack_slot_t *s = &state->inventory.slots[i];
        uint8_t *p = &rec[SLOTS_AT + SLOT_BYTES * i];
        p[0] = s->occupied ? 1u : 0u;
        p[1] = s->tool_id;
        p[2] = s->serial;
        p[3] = (uint8_t)(s->target_temp_c & 0xFFu);
        p[4] = (uint8_t)(s->target_temp_c >> 8);
    }
    rec[BODY_BYTES] = urtc_crc8(rec, BODY_BYTES);
}

// True when the record is complete and understood; `state` and `sequence` are written only then.
static bool decode(const uint8_t *rec, rack_state_t *state, uint32_t *sequence)
{
    if (rec[0] != RACK_PERSIST_MAGIC || rec[1] != RACK_PERSIST_VERSION) {
        return false;
    }
    if (rec[BODY_BYTES] != urtc_crc8(rec, BODY_BYTES)) {
        return false;
    }
    for (uint8_t i = 0; i < RACK_INVENTORY_MAX_SLOTS; i++) {
        if (rec[SLOTS_AT + SLOT_BYTES * i] > 1u) {
            return false;       // a flag that is neither 0 nor 1 is damage that the CRC happened to let through
        }
    }
    *sequence = get32(&rec[2]);
    state->lifecycle.total_cycles = get32(&rec[6]);
    state->lifecycle.total_seconds = get32(&rec[10]);
    for (uint8_t i = 0; i < RACK_INVENTORY_MAX_SLOTS; i++) {
        const uint8_t *p = &rec[SLOTS_AT + SLOT_BYTES * i];
        state->inventory.slots[i].occupied = (p[0] == 1u);
        state->inventory.slots[i].tool_id = p[1];
        state->inventory.slots[i].serial = p[2];
        state->inventory.slots[i].target_temp_c = (uint16_t)(p[3] | ((uint16_t)p[4] << 8));
    }
    return true;
}

typedef struct {
    bool valid;
    uint32_t sequence;
    rack_state_t state;
} copy_t;

static rack_persist_status_t read_copy(const rack_store_t *store, uint8_t index, copy_t *copy)
{
    uint8_t rec[RACK_PERSIST_RECORD_SIZE];
    copy->valid = false;
    if (!store->read(store->ctx, (uint32_t)index * RACK_PERSIST_COPY_STRIDE, rec, (uint8_t)RACK_PERSIST_RECORD_SIZE)) {
        return RACK_PERSIST_IO_ERROR;
    }
    copy->valid = decode(rec, &copy->state, &copy->sequence);
    return RACK_PERSIST_OK;
}

// True when `a` is newer than `b` (the sequence wraps after 2^32 saves; the difference is read as signed).
static bool newer(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) > 0;
}

// Reads both copies; `newest` is the index of the newest valid one, or -1.
static rack_persist_status_t scan(const rack_store_t *store, copy_t copies[COPIES], int *newest)
{
    *newest = -1;
    for (uint8_t i = 0; i < COPIES; i++) {
        const rack_persist_status_t status = read_copy(store, i, &copies[i]);
        if (status != RACK_PERSIST_OK) {
            return status;
        }
        if (copies[i].valid && (*newest < 0 || newer(copies[i].sequence, copies[*newest].sequence))) {
            *newest = (int)i;
        }
    }
    return RACK_PERSIST_OK;
}

rack_persist_status_t rack_persist_load(const rack_store_t *store, rack_state_t *state, uint32_t *sequence)
{
    if (store == NULL || store->read == NULL || state == NULL) {
        return RACK_PERSIST_BAD_ARGUMENT;
    }
    copy_t copies[COPIES];
    int newest;
    const rack_persist_status_t status = scan(store, copies, &newest);
    if (status != RACK_PERSIST_OK) {
        return status;
    }
    if (newest < 0) {
        return RACK_PERSIST_EMPTY;
    }
    *state = copies[newest].state;
    if (sequence != NULL) {
        *sequence = copies[newest].sequence;
    }
    return RACK_PERSIST_OK;
}

rack_persist_status_t rack_persist_save(const rack_store_t *store, const rack_state_t *state)
{
    if (store == NULL || store->read == NULL || store->write == NULL || state == NULL) {
        return RACK_PERSIST_BAD_ARGUMENT;
    }
    copy_t copies[COPIES];
    int newest;
    const rack_persist_status_t status = scan(store, copies, &newest);
    if (status != RACK_PERSIST_OK) {
        return status;
    }
    // Write the copy that is not the newest valid one (copy A on an empty memory).
    const uint8_t target = (newest == 0) ? 1u : 0u;
    const uint32_t sequence = (newest < 0) ? 1u : copies[newest].sequence + 1u;
    uint8_t rec[RACK_PERSIST_RECORD_SIZE];
    encode(rec, sequence, state);
    const uint32_t base = (uint32_t)target * RACK_PERSIST_COPY_STRIDE;
    static const uint8_t blank = 0u;
    // 1. take the target out of use, 2. write everything but the magic, 3. publish by writing the magic last.
    if (!store->write(store->ctx, base, &blank, 1u)) {
        return RACK_PERSIST_IO_ERROR;
    }
    if (!store->write(store->ctx, base + 1u, &rec[1], BODY_BYTES)) {
        return RACK_PERSIST_IO_ERROR;
    }
    if (!store->write(store->ctx, base, &rec[0], 1u)) {
        return RACK_PERSIST_IO_ERROR;
    }
    return RACK_PERSIST_OK;
}
