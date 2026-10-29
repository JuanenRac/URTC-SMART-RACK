// =============================================================================
// URTC-SMART-RACK Firmware - Single-producer / single-consumer byte ring: rack_ring.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The only thing a receive interrupt may do with a byte: put it here. The main loop takes the bytes out
// and feeds them to rack_deframer / rack_link, so link_watchdog_t and the deframer stay in ONE context
// (see rack_link.h). The ring is the one shared by the firmware repositories (urtc_ring.h, copied from
// URTC-COMMON); this file gives it the names of the rack.
#ifndef RACK_RING_H
#define RACK_RING_H

#include <stdbool.h>
#include <stdint.h>

#include "urtc_ring.h"

typedef urtc_ring_t rack_ring_t;

// `size` must be a power of two between 2 and 32768; one slot stays free, so a ring of `size` bytes holds `size - 1`.
// Returns false (and leaves the ring unusable) otherwise.
bool rack_ring_init(rack_ring_t *ring, uint8_t *storage, uint16_t size);

// Producer side (interrupt). Returns false when the ring is full and the byte was dropped (and counted).
bool rack_ring_put(rack_ring_t *ring, uint8_t byte);

// Consumer side (main loop). Returns false when there is nothing to read.
bool rack_ring_get(rack_ring_t *ring, uint8_t *out);

// Bytes waiting to be read (a snapshot: the producer may add more at any moment).
uint16_t rack_ring_count(const rack_ring_t *ring);

// Bytes dropped since the last call; consumer side.
uint16_t rack_ring_take_dropped(rack_ring_t *ring);

#endif // RACK_RING_H
