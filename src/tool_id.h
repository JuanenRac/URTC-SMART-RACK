// =============================================================================
// URTC-SMART-RACK Firmware - Tool ID decoding: tool_id.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// Pure logic, no hardware access: decoding a raw 5-bit ID jumper reading
// (or an F-RAM-stored byte, once that exists - see README's "Tool
// Tracking" Key Feature) into a tool identity is independent of how those
// bits were actually read, so it's real and unit-testable today even
// though there's no PCB yet to wire real ID jumpers or F-RAM to.
#ifndef TOOL_ID_H
#define TOOL_ID_H

#include <stdbool.h>
#include <stdint.h>

#define TOOL_ID_BITS 5
#define TOOL_ID_MASK ((1u << TOOL_ID_BITS) - 1u) // 0x1F

// All-ones (0x1F) is the reading a 5-bit ID bus gives when nothing is
// plugged in and every line floats high on its pull-up - reserved as
// "no tool present" rather than a valid tool slot, same convention as
// URTC's own tool-head ID scheme.
#define TOOL_ID_NONE TOOL_ID_MASK

// Masks a raw GPIO/F-RAM reading down to the 5 bits that carry the tool
// ID, ignoring any other bits the same byte/register might carry.
uint8_t tool_id_decode(uint8_t raw_bits);

// True for any decoded ID except TOOL_ID_NONE.
bool tool_id_is_present(uint8_t tool_id);

#endif // TOOL_ID_H
