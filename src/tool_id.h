// =============================================================================
// URTC-SMART-RACK Firmware - Tool ID decoding: tool_id.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// Pure logic, no hardware access: decoding a raw 5-bit ID jumper reading
// (or an F-RAM-stored byte, once that exists - see README's "Tool
// Tracking" Key Feature) into a tool identity is independent of how those
// bits were actually read, so it's real and unit-testable today even
// though there's no PCB yet to wire real ID jumpers or F-RAM to.
//
// The ID space is the one of the URTC tool heads (URTC's ToolMode_t):
//   0..24   a tool profile (TOOL_ID_URTC_FIRST..TOOL_ID_URTC_LAST)
//   25..30  invalid: no tool profile has been assigned to them
//   31      "free configuration" on a URTC head: the real tool is chosen by a
//           value stored in the head's F-RAM, so the five bits alone do not
//           identify it
// Polarity: the bits handed to tool_id_decode() are LOGICAL bits, where an
// installed jumper (line pulled to ground) reads as 1, exactly as a URTC head
// reads its own ID. A caller reading raw pin levels must invert them first;
// a head with no jumper at all is therefore tool 0, not "absent".
//
// Whether a slot holds a tool at all is NOT decided by this ID: that comes
// from the per-slot presence sensors (see rack_inventory_reconcile()).
// TOOL_ID_NONE below is only this firmware's internal marker for "this slot
// holds nothing".
#ifndef TOOL_ID_H
#define TOOL_ID_H

#include <stdbool.h>
#include <stdint.h>

#include "urtc_tool_ids.h" // the numbering of the tool profiles, generated once for every repository that uses it

#define TOOL_ID_BITS 5
#define TOOL_ID_MASK URTC_TOOL_ID_MASK // 0x1F: five bits

// First and last tool profile of the URTC head catalogue.
#define TOOL_ID_URTC_FIRST 0u
#define TOOL_ID_URTC_LAST URTC_TOOL_ID_LAST

// Value 31 on a URTC head means "free configuration" (the tool is read from
// the head's F-RAM). The rack cannot pre-heat such a head by its ID alone, so
// it is never accepted as a tool to act on: free_tool.h asks the head for its
// tool (0x1A3) and gives the slot an identity only from a valid answer.
// Internally the same value marks an empty slot.
#define TOOL_ID_FREE_CONFIG URTC_TOOL_ID_FREE_CONFIG
#define TOOL_ID_NONE TOOL_ID_MASK

// Masks a raw GPIO/F-RAM reading down to the 5 bits that carry the tool
// ID, ignoring any other bits the same byte/register might carry. Use it for
// hardware readings; a byte that arrives in a protocol frame must NOT be
// masked (see tool_id_is_wire_byte_valid()).
uint8_t tool_id_decode(uint8_t raw_bits);

// True for any decoded ID except TOOL_ID_NONE.
bool tool_id_is_present(uint8_t tool_id);

// True for an ID that names a tool profile of the URTC catalogue (0..24).
bool tool_id_is_valid_urtc(uint8_t tool_id);

// True when a tool byte received over the link is a plain 5-bit value: a byte
// with any of the three upper bits set is corruption or another protocol
// version, not a tool.
bool tool_id_is_wire_byte_valid(uint8_t wire_byte);

#endif // TOOL_ID_H
