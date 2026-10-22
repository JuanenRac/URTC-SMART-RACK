// =============================================================================
// URTC-SMART-RACK Firmware - Thermal check of a slot: slot_verify.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "slot_verify.h"

slot_verdict_t slot_verify(bool occupied, uint16_t commanded_c, float measured_c, float ambient_c, uint32_t elapsed_ms)
{
    if (measured_c != measured_c) { // NaN
        return SLOT_VERDICT_UNKNOWN;
    }
    if (!occupied) {
        return (measured_c > ambient_c + SLOT_VERIFY_EMPTY_HOT_C) ? SLOT_VERDICT_EMPTY_HOT : SLOT_VERDICT_EMPTY_OK;
    }
    if (commanded_c == 0u) {
        return (measured_c > ambient_c + SLOT_VERIFY_IDLE_HOT_C) ? SLOT_VERDICT_IDLE_HOT : SLOT_VERDICT_IDLE_OK;
    }
    const float target = (float)commanded_c;
    if (measured_c > target + SLOT_VERIFY_OVERSHOOT_C) {
        return SLOT_VERDICT_OVERSHOOT;
    }
    if (measured_c >= target - SLOT_VERIFY_TOLERANCE_C) {
        return SLOT_VERDICT_REACHED;
    }
    return (elapsed_ms >= SLOT_VERIFY_REACH_TIMEOUT_MS) ? SLOT_VERDICT_NOT_REACHED : SLOT_VERDICT_HEATING;
}

bool slot_verdict_is_fault(slot_verdict_t verdict)
{
    return verdict == SLOT_VERDICT_EMPTY_HOT || verdict == SLOT_VERDICT_IDLE_HOT || verdict == SLOT_VERDICT_NOT_REACHED ||
           verdict == SLOT_VERDICT_OVERSHOOT;
}
