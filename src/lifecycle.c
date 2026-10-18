// =============================================================================
// URTC-SMART-RACK Firmware - Tool lifecycle tracking: lifecycle.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "lifecycle.h"
#include <stddef.h>

void lifecycle_init(lifecycle_t *lc)
{
    if (lc == NULL) {
        return;
    }
    lc->total_cycles = 0;
    lc->total_seconds = 0;
}

void lifecycle_record_use(lifecycle_t *lc, uint32_t duration_seconds)
{
    if (lc == NULL) {
        return;
    }
    // Saturate instead of wrapping: a wrapped counter would make a worn tool look new again.
    lc->total_cycles = (lc->total_cycles == UINT32_MAX) ? UINT32_MAX : lc->total_cycles + 1u;
    lc->total_seconds = (duration_seconds > UINT32_MAX - lc->total_seconds) ? UINT32_MAX : lc->total_seconds + duration_seconds;
}

bool lifecycle_needs_maintenance(const lifecycle_t *lc, uint32_t max_cycles)
{
    if (lc == NULL) {
        return false;
    }
    return lc->total_cycles >= max_cycles;
}
