// =============================================================================
// URTC-SMART-RACK Firmware - The frames between the rack and the head in its slot: head_link.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The rack does not heat anything: the head does, and it answers to the same CAN frames as for any other host. This module is what the rack says to
// a head and how it reads the answer, as pure functions that build a frame (identifier, length, bytes) or read one. Nothing here transmits: the
// caller owns the CAN peripheral and the clock, and no rack board with a transceiver exists yet.
//
//   * What the head is and whether it is well: 0x110 asks, 0x111 answers (tool, system and CAN error flags, booting, thermal fault, reset cause).
//   * How worn it is: 0x1AA asks, 0x1AB answers (fault count, seconds the heater was on, last fault, whether the head keeps them in its F-RAM);
//     rack_units_note_counters() takes those numbers.
//   * Pre-heat: an order of the plan (a tool and a target) becomes the ordinary setpoint frame of that tool, 0x130 for the soldering iron and 0x1E0 for
//     hot air (a blower duty goes with it). The head switches its heater off by itself when the setpoint stops arriving, so the frame is sent again
//     every refresh_ms for as long as the order stands, and at once when the target changes. A target of 0 is sent too: it is the order to cool.
//     Tools without a heater the rack warms up have no frame (head_link_build_setpoint() returns false).
//
// The times and the blower duty live in head_link_config_t (defaults below), never in the logic. The values are estimates for a bench to confirm.
// The identifiers are the ones of the specification (URTC-COMMON checks this file against it).
#ifndef HEAD_LINK_H
#define HEAD_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "urtc_tool_ids.h"

#define HEAD_LINK_ID_QUERY_ACTIVE_TOOL 0x110u
#define HEAD_LINK_ID_ACTIVE_TOOL_RESP 0x111u
#define HEAD_LINK_ID_SOLDER_SETPOINT 0x130u
#define HEAD_LINK_ID_THERMAL_STATS_QUERY 0x1AAu
#define HEAD_LINK_ID_THERMAL_STATS_RESP 0x1ABu
#define HEAD_LINK_ID_HOTAIR_CMD 0x1E0u


#define HEAD_LINK_DEFAULT_REFRESH_MS 1000u
#define HEAD_LINK_DEFAULT_HOTAIR_BLOWER_DUTY 128u
#define HEAD_LINK_DEFAULT_COUNTERS_PERIOD_MS 60000u

typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
} head_frame_t;

typedef struct {
    uint32_t setpoint_refresh_ms;   // the setpoint is sent again this often while the order stands
    uint8_t hotair_blower_duty;     // 0 to 255, sent with every hot-air setpoint
    uint32_t counters_period_ms;    // the wear counters of a head are read this often (0 = never)
} head_link_config_t;

head_link_config_t head_link_config_default(void);

// The two questions (no payload).
bool head_link_build_query_active_tool(head_frame_t *out);
bool head_link_build_query_counters(head_frame_t *out);

// The setpoint frame of a tool: big-endian target in degrees C. False for a tool the rack has no frame for, or a NULL argument.
bool head_link_build_setpoint(const head_link_config_t *config, uint8_t tool_id, uint16_t target_c, head_frame_t *out);

typedef struct {
    uint8_t tool_id;
    bool system_error;
    bool can_error;
    bool booting;
    uint8_t thermal_fault;   // 0 = none; the fault code otherwise (0xFF when the head did not say)
    uint8_t reset_cause;     // 0xFF when the head did not say
} head_state_t;

// Reads the answer of 0x110 (identifier 0x111, four to six bytes). False for another identifier or a short frame; nothing is written then.
bool head_link_parse_active_tool(uint32_t id, const uint8_t *data, uint8_t dlc, head_state_t *out);

typedef struct {
    uint16_t fault_count;
    uint32_t heater_on_seconds;
    uint8_t last_fault;
    bool persisted;          // the head keeps the counters in its F-RAM (false: it counted in RAM since boot)
} head_counters_t;

// Reads the answer of 0x1AA (identifier 0x1AB, eight bytes). False for another identifier or another length.
bool head_link_parse_counters(uint32_t id, const uint8_t *data, uint8_t dlc, head_counters_t *out);

// When to send the setpoint again. One per slot; start with all zeros.
typedef struct {
    bool sent_once;
    uint16_t last_target_c;
    uint32_t last_sent_ms;
} head_setpoint_state_t;

// True when a setpoint frame must go out now: the first one, a changed target, or the refresh time has passed (the clock may wrap). It records that it was sent.
bool head_link_setpoint_due(const head_link_config_t *config, head_setpoint_state_t *state, uint16_t target_c, uint32_t now_ms);

// Forget what was sent (the head left the slot, the link was lost): the next call of head_link_setpoint_due() says yes.
void head_link_setpoint_reset(head_setpoint_state_t *state);

#endif // HEAD_LINK_H
