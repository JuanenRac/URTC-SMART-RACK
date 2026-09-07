// =============================================================================
// URTC-SMART-RACK Firmware - Rack command validation: rack_command.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// Turns a real, CRC-valid protocol_frame_t (see protocol.h) into a real,
// range-checked command - the promotion audit's own "aplicar limites de
// actuacion" - as its own module rather than inline wherever a future
// CAN/UART receive interrupt eventually calls it. A frame can be
// perfectly well-formed at the protocol level and still ask for something
// this firmware must refuse (an out-of-range temperature, an absent tool
// slot); that judgment belongs here, not the wire-framing layer.
#ifndef RACK_COMMAND_H
#define RACK_COMMAND_H

#include <stdint.h>

typedef enum {
    RACK_CMD_SET_PREHEAT = 0x01,
} rack_command_id_t;

typedef enum {
    RACK_CMD_OK = 0,
    RACK_CMD_ERR_WRONG_PAYLOAD_LEN,
    RACK_CMD_ERR_TOOL_ID_ABSENT,
    RACK_CMD_ERR_TEMP_OUT_OF_RANGE,
} rack_command_status_t;

// Real, safe upper bound on any commanded preheat target - above the
// hottest real tool type this firmware knows about today
// (TOOL_TYPE_HOT_AIR, 350C - see preheat.c) with a real margin, not an
// arbitrary round number picked without that reference.
#define RACK_COMMAND_MAX_TEMP_C 400u

typedef struct {
    uint8_t tool_id;
    uint16_t target_temp_c;
} rack_set_preheat_t;

// Decodes and range-checks a RACK_CMD_SET_PREHEAT payload
// (tool_id: 1 byte, target_temp_c: 2 bytes little-endian - 3 bytes total).
// Rejects a payload of the wrong length, an absent tool slot (see
// tool_id.h's TOOL_ID_NONE), or a temperature above
// RACK_COMMAND_MAX_TEMP_C - `out_command` is only written on RACK_CMD_OK.
rack_command_status_t rack_command_validate_set_preheat(const uint8_t *payload, uint8_t len, rack_set_preheat_t *out_command);

#endif // RACK_COMMAND_H
