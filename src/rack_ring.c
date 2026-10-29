// =============================================================================
// URTC-SMART-RACK Firmware - Single-producer / single-consumer byte ring: rack_ring.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "rack_ring.h"
#include <stddef.h>

// Release/acquire access to the indices. On a Cortex-M this is a plain load/store plus a DMB, and it is
// also correct for the compilers that run the host tests (they may reorder plain accesses otherwise).
#define RING_LOAD_ACQUIRE(p)       __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define RING_STORE_RELEASE(p, v)   __atomic_store_n((p), (v), __ATOMIC_RELEASE)

bool rack_ring_init(rack_ring_t *ring, uint8_t *storage, uint16_t size)
{
    if (ring == NULL) {
        return false;
    }
    ring->storage = NULL;
    ring->mask = 0u;
    ring->head = 0u;
    ring->tail = 0u;
    ring->dropped = 0u;
    ring->dropped_seen = 0u;
    if (storage == NULL || size < 2u || size > 32768u || (size & (uint16_t)(size - 1u)) != 0u) {
        return false;
    }
    ring->storage = storage;
    ring->mask = (uint16_t)(size - 1u);
    return true;
}

bool rack_ring_put(rack_ring_t *ring, uint8_t byte)
{
    if (ring == NULL || ring->storage == NULL) {
        return false;
    }
    const uint16_t head = ring->head;                          // only this side writes it
    const uint16_t next = (uint16_t)((head + 1u) & ring->mask);
    if (next == RING_LOAD_ACQUIRE(&ring->tail)) {
        ring->dropped = (uint16_t)(ring->dropped + 1u);
        return false;
    }
    ring->storage[head] = byte;
    RING_STORE_RELEASE(&ring->head, next);                     // the byte is visible before the index
    return true;
}

bool rack_ring_get(rack_ring_t *ring, uint8_t *out)
{
    if (ring == NULL || ring->storage == NULL || out == NULL) {
        return false;
    }
    const uint16_t tail = ring->tail;                          // only this side writes it
    if (tail == RING_LOAD_ACQUIRE(&ring->head)) {
        return false;
    }
    *out = ring->storage[tail];
    RING_STORE_RELEASE(&ring->tail, (uint16_t)((tail + 1u) & ring->mask));
    return true;
}

uint16_t rack_ring_count(const rack_ring_t *ring)
{
    if (ring == NULL || ring->storage == NULL) {
        return 0u;
    }
    const uint16_t head = RING_LOAD_ACQUIRE(&ring->head);
    const uint16_t tail = RING_LOAD_ACQUIRE(&ring->tail);
    return (uint16_t)((head - tail) & ring->mask);
}

uint16_t rack_ring_take_dropped(rack_ring_t *ring)
{
    if (ring == NULL) {
        return 0u;
    }
    const uint16_t dropped = RING_LOAD_ACQUIRE(&ring->dropped);
    const uint16_t since = (uint16_t)(dropped - ring->dropped_seen);
    ring->dropped_seen = dropped;
    return since;
}
