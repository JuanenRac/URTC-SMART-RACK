// =============================================================================
// URTC-SMART-RACK Firmware - Real receive-path decision: rack_link.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The real frame-dispatch decision tying protocol/rack-command/link-watchdog together, as a module a real UART/CAN receive interrupt can call (the tests call INTO it).
//
// It answers with a DECISION, not with a temperature. It used to return one number that meant two things at once - "apply this temperature" and "this frame does not count" - and
// a rejected frame (noise on the CRC, an honest retransmission with the same sequence, a command that is not SET_PREHEAT) came out as 0, which is "switch the heater off". With
// two tools in the rack it also did not say WHICH tool the number was for. A frame that is not a command to act on now changes nothing; only a refused value, or a link the
// watchdog says is lost, asks for the safe state, and the decision always names the tool.
#ifndef RACK_LINK_H
#define RACK_LINK_H

#include <stdint.h>

#include "link_watchdog.h"

typedef enum {
    RACK_ACT_NONE = 0,      // keep every target as it is: a corrupt frame, a retransmission already applied, a command this rack does not act on
    RACK_ACT_SET_TARGET,    // apply `temp_c` to the tool `tool_id`
    RACK_ACT_SAFE_STATE,    // bring the tool `tool_id` to the safe state (`temp_c` is that temperature): its command asked for something outside what the rack allows
} rack_action_t;

typedef struct {
    rack_action_t action;
    uint8_t tool_id;        // meaningful for SET_TARGET and SAFE_STATE
    uint16_t temp_c;        // meaningful for SET_TARGET (the requested target) and SAFE_STATE (the safe temperature)
    uint8_t acknowledge;    // 1 when the sender should be told the frame arrived (a command applied, or a retransmission of one that already was); 0 otherwise
} rack_decision_t;

// Execution context: the watchdog state is plain, non-atomic memory. Call this
// function and every link_watchdog_* function from ONE context - the main loop.
// A receive interrupt must only copy the received bytes into a single-producer
// ring buffer (volatile indices, a memory barrier between the data and the index
// that publishes it); two interrupts of different priority, or an interrupt and
// the main loop, must never touch the same link_watchdog_t.
//
// The decision for one incoming buffer. `lw` is mutated exactly as a real receive interrupt handler's own watchdog state would be. A link that the watchdog already considers
// lost is handled by the caller's own periodic check (link_watchdog_is_link_lost() -> safe state for every tool): a frame arriving does not hide that.
void rack_link_decide(link_watchdog_t *lw, const uint8_t *buf, uint8_t buf_len, uint32_t now_ms, rack_decision_t *out);

#endif // RACK_LINK_H
