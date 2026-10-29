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

#include <stdbool.h>

#include "link_watchdog.h"
#include "rack_deframer.h"

typedef enum {
    RACK_ACT_NONE = 0,      // keep every target as it is: a corrupt frame, a retransmission already applied, a command this rack does not act on
    RACK_ACT_SET_TARGET,    // apply `temp_c` to the tool `tool_id`
    RACK_ACT_SAFE_STATE,    // bring the tool `tool_id` to the safe state (`temp_c` is that temperature): its command asked for something outside what the rack allows
} rack_action_t;

// Why a frame was applied or refused, in the one byte the sender is told. Zero and one are the two kinds of acknowledgement; the rest say what to change. A frame that never
// reaches command validation (corrupt framing, not for this rack) is RACK_REPLY_NONE: nobody is told, because noise must not provoke an answer.
typedef enum {
    RACK_REPLY_APPLIED = 0,             // the command was applied
    RACK_REPLY_REPEAT = 1,              // already applied (a retransmission or a stale sequence): nothing to do, stop resending
    RACK_REPLY_WRONG_LENGTH = 2,        // the payload is not the size the command has
    RACK_REPLY_TOOL_ABSENT = 3,         // the tool byte says the slot holds nothing
    RACK_REPLY_TEMP_OUT_OF_RANGE = 4,   // the target is above what the rack (or that tool) accepts
    RACK_REPLY_TOOL_INVALID = 5,        // not a plain five-bit value, or no tool profile has that number
    RACK_REPLY_NOT_HEATABLE = 6,        // a target above zero for a tool that has no heater
    RACK_REPLY_HELD_RATE = 7,           // too soon after the previous command accepted for that tool: send it again later
    RACK_REPLY_UNKNOWN_COMMAND = 8,     // a well-formed frame with a command this rack does not have
    RACK_REPLY_NONE = 0xFF              // no answer is sent
} rack_reply_code_t;
typedef struct {
    rack_action_t action;
    uint8_t tool_id;        // meaningful for SET_TARGET and SAFE_STATE
    uint16_t temp_c;        // meaningful for SET_TARGET (the requested target) and SAFE_STATE (the safe temperature)
    uint8_t acknowledge;    // 1 when the sender should be told the frame arrived (a command applied, or a retransmission of one that already was); 0 otherwise
    uint8_t reply;          // rack_reply_code_t: the reason to tell the sender (RACK_REPLY_NONE when nobody is told); acknowledge is 1 exactly for APPLIED and REPEAT
    uint8_t reply_cmd;      // the command the reply is about
    uint8_t reply_tool;     // the tool byte the frame carried, as it was sent (0xFF when the command has none)
    uint8_t reply_seq;      // the sequence number of the frame the reply is about
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

// The same decision for a command that arrived as a CAN frame (see rack_can.h: identifier, 8 data bytes, big-endian temperature). It shares every rule with the serial path: the
// watchdog bookkeeping, the validation of the command, the anti-replay sequence. A frame with another identifier or another length is not ours and changes nothing.
void rack_link_decide_can(link_watchdog_t *lw, uint32_t can_id, const uint8_t *data, uint8_t dlc, uint32_t now_ms, rack_decision_t *out);

// Serial receive path: feeds one received byte to the deframer and, when that completes a frame, fills `out` with its decision and returns true. A byte that completes nothing
// returns false and leaves `out` untouched. After a true return call rack_link_rx_next() until it returns false: after a resynchronisation the deframer can hold a second frame.
bool rack_link_rx_byte(rack_deframer_t *rx, link_watchdog_t *lw, uint8_t byte, uint32_t now_ms, rack_decision_t *out);
bool rack_link_rx_next(rack_deframer_t *rx, link_watchdog_t *lw, uint32_t now_ms, rack_decision_t *out);

#endif // RACK_LINK_H
