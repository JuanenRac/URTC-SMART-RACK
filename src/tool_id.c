// =============================================================================
// URTC-SMART-RACK Firmware - Tool ID decoding: tool_id.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "tool_id.h"

uint8_t tool_id_decode(uint8_t raw_bits)
{
    return (uint8_t)(raw_bits & TOOL_ID_MASK);
}

bool tool_id_is_present(uint8_t tool_id)
{
    return tool_id != TOOL_ID_NONE;
}

bool tool_id_is_valid_urtc(uint8_t tool_id)
{
    return tool_id <= TOOL_ID_URTC_LAST;
}

bool tool_id_is_wire_byte_valid(uint8_t wire_byte)
{
    return wire_byte <= TOOL_ID_MASK;
}
