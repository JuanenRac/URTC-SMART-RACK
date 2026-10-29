// =============================================================================
// URTC-SMART-RACK Firmware - Single-producer / single-consumer byte ring: rack_ring.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The only thing a receive interrupt may do with a byte: put it here. The main loop takes the bytes out
// and feeds them to rack_deframer / rack_link, so link_watchdog_t and the deframer stay in ONE context
// (see rack_link.h). One producer (the interrupt) and one consumer (the main loop) never need a lock:
// the producer owns `head`, the consumer owns `tail`, and each index is published with a release store
// and read with an acquire load, so the byte is always visible before the index that announces it.
//
// The size must be a power of two (the indices wrap by masking). One slot is kept free to tell "full"
// from "empty", so a ring of `size` bytes holds `size - 1`. A byte that does not fit is dropped and
// counted: the deframer then sees a gap, which its length and CRC checks already turn into a lost frame.
#ifndef RACK_RING_H
#define RACK_RING_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t *storage;
    uint16_t mask;                // size - 1
    volatile uint16_t head;       // next index to write; written only by the producer
    volatile uint16_t tail;       // next index to read; written only by the consumer
    volatile uint16_t dropped;    // running count of bytes that did not fit (wraps); written only by the producer
    uint16_t dropped_seen;        // the value of `dropped` the consumer already reported; written only by the consumer
} rack_ring_t;

// `size` must be a power of two between 2 and 32768. Returns false (and leaves the ring unusable) otherwise.
bool rack_ring_init(rack_ring_t *ring, uint8_t *storage, uint16_t size);

// Producer side (interrupt). Returns false when the ring is full and the byte was dropped.
bool rack_ring_put(rack_ring_t *ring, uint8_t byte);

// Consumer side (main loop). Returns false when there is nothing to read.
bool rack_ring_get(rack_ring_t *ring, uint8_t *out);

// Bytes waiting to be read (a snapshot: the producer may add more at any moment).
uint16_t rack_ring_count(const rack_ring_t *ring);

// Bytes dropped since the last call; consumer side. (More than 65535 drops between two calls are under-counted.)
uint16_t rack_ring_take_dropped(rack_ring_t *ring);

#endif // RACK_RING_H
