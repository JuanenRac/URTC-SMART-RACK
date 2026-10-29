// =============================================================================
// URTC-SMART-RACK Firmware - Single-producer / single-consumer byte ring: rack_ring.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "rack_ring.h"

bool rack_ring_init(rack_ring_t *ring, uint8_t *storage, uint16_t size)
{
    return urtc_ring_init(ring, storage, size);
}

bool rack_ring_put(rack_ring_t *ring, uint8_t byte)
{
    return urtc_ring_put(ring, byte);
}

bool rack_ring_get(rack_ring_t *ring, uint8_t *out)
{
    return urtc_ring_get(ring, out);
}

uint16_t rack_ring_count(const rack_ring_t *ring)
{
    return urtc_ring_count(ring);
}

uint16_t rack_ring_take_dropped(rack_ring_t *ring)
{
    return urtc_ring_take_dropped(ring);
}
