// =============================================================================
// URTC-SMART-RACK Firmware - Thermal check of a slot: slot_verify.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// A thermal camera that looks at the tips in the rack gives one temperature per slot. This decides what that reading says about
// the pre-heat the rack ordered: has the tip reached its target, is it too slow, too hot, or is a heater on in a slot that has no
// tool in it. It is pure logic: the reading comes from the camera tool (URTC-VISION-TOOL) and the verdict goes to the host; the
// rack never acts on it by itself. The limits are estimates for a soldering tip, to be measured on a bench.
#ifndef SLOT_VERIFY_H
#define SLOT_VERIFY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SLOT_VERDICT_UNKNOWN = 0,      // no reading (NaN)
    SLOT_VERDICT_EMPTY_OK,
    SLOT_VERDICT_EMPTY_HOT,        // an empty slot that is hot: a heater is on with no tool in it
    SLOT_VERDICT_IDLE_OK,
    SLOT_VERDICT_IDLE_HOT,         // told to stay cold and still hot
    SLOT_VERDICT_HEATING,
    SLOT_VERDICT_REACHED,
    SLOT_VERDICT_NOT_REACHED,      // still short of the target after the allowed time
    SLOT_VERDICT_OVERSHOOT,
} slot_verdict_t;

#define SLOT_VERIFY_TOLERANCE_C 15.0f
#define SLOT_VERIFY_OVERSHOOT_C 25.0f
#define SLOT_VERIFY_EMPTY_HOT_C 40.0f
#define SLOT_VERIFY_IDLE_HOT_C 60.0f
#define SLOT_VERIFY_REACH_TIMEOUT_MS 60000u

// `elapsed_ms` is the time since the target was set. A reading that is NaN gives SLOT_VERDICT_UNKNOWN.
slot_verdict_t slot_verify(bool occupied, uint16_t commanded_c, float measured_c, float ambient_c, uint32_t elapsed_ms);

// True for the verdicts a host should treat as a fault.
bool slot_verdict_is_fault(slot_verdict_t verdict);

#endif // SLOT_VERIFY_H
