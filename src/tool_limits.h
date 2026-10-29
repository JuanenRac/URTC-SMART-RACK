// =============================================================================
// URTC-SMART-RACK Firmware - Per-tool pre-heat limits: tool_limits.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The highest pre-heat target the rack accepts depends on the tool: a head
// that URTC itself never lets above 310 C must not be asked for 390 C, and a
// head with no heater (a gripper, a screwdriver, a camera) has nothing to
// pre-heat at all. The table mirrors the heated tool profiles of URTC's tool
// catalogue (its ToolMode_t numbering, see tool_id.h); every other profile has
// no heater as far as that firmware is concerned. Adding a heated profile
// means adding one line to the table in tool_limits.c, together with a test.
#ifndef TOOL_LIMITS_H
#define TOOL_LIMITS_H

#include <stdint.h>

#include "urtc_tool_ids.h"

// Ceiling that applies to every tool, whatever its own limit: the rack never
// asks for more than this (same value as RACK_COMMAND_MAX_TEMP_C).
#define TOOL_LIMITS_RACK_CEILING_C 400u

// Tool profiles of the URTC catalogue that carry a heater.
#define TOOL_ID_SOLDERING_IRON URTC_TOOL_ID_SOLDERING_IRON
#define TOOL_ID_3D_PRINTER URTC_TOOL_ID_PRINTER_3D
#define TOOL_ID_HOTAIR_REWORK URTC_TOOL_ID_HOTAIR_REWORK

// Highest pre-heat target (whole degrees C) the rack accepts for the tool, or
// 0 when the tool has no heater - and for an ID that is not a tool profile at
// all - meaning that no target above 0 is accepted for it.
uint16_t tool_limits_max_preheat_c(uint8_t tool_id);

#endif // TOOL_LIMITS_H
