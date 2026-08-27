// =============================================================================
// URTC-SMART-RACK Firmware - Smart Idle pre-heat logic: preheat.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "preheat.h"

uint16_t preheat_target_temp_c(tool_type_t type)
{
    switch (type) {
        case TOOL_TYPE_SOLDERING_IRON:
            return 200; // Matches the README's own SMART RACK WORKFLOW diagram.
        case TOOL_TYPE_HOT_AIR:
            return 350;
        case TOOL_TYPE_GENERIC:
        default:
            return 0;
    }
}

bool preheat_should_activate(uint32_t ms_until_next_use, uint32_t lead_time_ms)
{
    if (ms_until_next_use == 0) {
        return false;
    }
    return ms_until_next_use <= lead_time_ms;
}
