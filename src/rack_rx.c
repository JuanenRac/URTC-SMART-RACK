// =============================================================================
// URTC-SMART-RACK Firmware - Interrupt-safe serial receive path: rack_rx.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "rack_rx.h"

#include <stddef.h>

void rack_rx_init(rack_rx_t *rx, uint32_t timeout_ms)
{
    if (rx == NULL) {
        return;
    }
    (void)rack_ring_init(&rx->ring, rx->ring_storage, RACK_RX_RING_SIZE);
    rack_deframer_init(&rx->deframer, timeout_ms);
    rx->frames_pending = false;
    rx->overflow_bytes = 0u;
}

bool rack_rx_on_byte(rack_rx_t *rx, uint8_t byte)
{
    if (rx == NULL) {
        return false;
    }
    return rack_ring_put(&rx->ring, byte);
}

bool rack_rx_poll(rack_rx_t *rx, link_watchdog_t *lw, uint32_t now_ms, rack_decision_t *out)
{
    if (rx == NULL || lw == NULL || out == NULL) {
        return false;
    }
    rx->overflow_bytes += rack_ring_take_dropped(&rx->ring);
    if (rx->frames_pending) {
        if (rack_link_rx_next(&rx->deframer, lw, now_ms, out)) {
            return true;
        }
        rx->frames_pending = false;
    }
    uint8_t byte;
    while (rack_ring_get(&rx->ring, &byte)) {
        if (rack_link_rx_byte(&rx->deframer, lw, byte, now_ms, out)) {
            rx->frames_pending = true;   // after a resynchronisation the deframer can hold a second frame
            return true;
        }
    }
    return false;
}
