// =============================================================================
// URTC-SMART-RACK Firmware - Per-tool pre-heat limits: tool_limits.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
#include "tool_limits.h"
#include "tool_id.h"

// Indexed by tool ID. A soldering iron (T12) and the hot-air head can go
// beyond the rack's own ceiling in URTC (445 C set point), so the rack
// ceiling is the limit that applies to them; the 3D printer hotend is held to
// 310 C by URTC's thermal guard. Everything else has no heater.
static const uint16_t k_max_preheat_c[TOOL_ID_URTC_LAST + 1u] = {
    [TOOL_ID_SOLDERING_IRON] = TOOL_LIMITS_RACK_CEILING_C,
    [TOOL_ID_3D_PRINTER] = 310u,
    [TOOL_ID_HOTAIR_REWORK] = TOOL_LIMITS_RACK_CEILING_C,
};

uint16_t tool_limits_max_preheat_c(uint8_t tool_id)
{
    if (!tool_id_is_valid_urtc(tool_id)) {
        return 0u;
    }
    return k_max_preheat_c[tool_id];
}
