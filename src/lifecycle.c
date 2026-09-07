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
    lc->total_cycles += 1;
    lc->total_seconds += duration_seconds;
}

bool lifecycle_needs_maintenance(const lifecycle_t *lc, uint32_t max_cycles)
{
    if (lc == NULL) {
        return false;
    }
    return lc->total_cycles >= max_cycles;
}
