// =============================================================================
// URTC-SMART-RACK Firmware - Interrupt-safe serial receive path: rack_rx.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The shape the serial receive path takes once the link has an interrupt: the interrupt (or the DMA handler) calls ONLY rack_rx_on_byte(), which puts the
// byte in a single-producer ring (rack_ring.h); the main loop calls rack_rx_poll(), which takes the bytes out, finds the frames in them with the deframer
// and gives each one's decision (rack_link.h). So the deframer and the link watchdog are touched from ONE context, and a receive interrupt can never be
// in the middle of a sequence number while the main loop reads it.
//
//   interrupt:   rack_rx_on_byte(&rx, byte);
//   main loop:   while (rack_rx_poll(&rx, &lw, now_ms, &decision)) { rack_app_apply_decision(&inventory, &decision); }
//                rack_app_tick(&inventory, &lw, now_ms);
//
// No interrupt exists yet (no board, no transport): the host tests play the bytes of the scenarios through it, and tests/isr_simulator.c delivers them from a
// signal handler that preempts the main loop.
#ifndef RACK_RX_H
#define RACK_RX_H

#include <stdbool.h>
#include <stdint.h>

#include "link_watchdog.h"
#include "rack_deframer.h"
#include "rack_link.h"
#include "rack_ring.h"

#define RACK_RX_RING_SIZE 64u   // a power of two; holds two whole frames

typedef struct {
    rack_ring_t ring;
    uint8_t ring_storage[RACK_RX_RING_SIZE];
    rack_deframer_t deframer;       // main loop only
    bool frames_pending;            // main loop only: the deframer may hold another frame after a resynchronisation
    uint32_t overflow_bytes;        // main loop only: bytes the ring had no room for (they show up as a lost frame)
} rack_rx_t;

// `timeout_ms` is the inter-byte timeout of the deframer (0 takes its default).
void rack_rx_init(rack_rx_t *rx, uint32_t timeout_ms);

// Interrupt side: the only call an interrupt may make. Returns false when the byte was dropped.
bool rack_rx_on_byte(rack_rx_t *rx, uint8_t byte);

// Main-loop side: takes bytes out of the ring until one completes a frame, fills `out` with its decision and returns true; call it again for the bytes
// that are left (and for a second frame the deframer kept after a resynchronisation). Returns false when nothing more is complete.
bool rack_rx_poll(rack_rx_t *rx, link_watchdog_t *lw, uint32_t now_ms, rack_decision_t *out);

#endif // RACK_RX_H
