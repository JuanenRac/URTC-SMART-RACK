// =============================================================================
// URTC-SMART-RACK Firmware - CAN mapping of the rack command: rack_can.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The route the rack command will take once a board with a CAN transceiver exists. No such
// board exists today: the serial framing of protocol.h is the transport that is implemented,
// and this module only fixes how the same command looks on the URTC bus so that the byte
// order and the size are decided before the hardware, not after it. It is pure logic with a
// host test, like the rest of the rack.
//
// A classic CAN frame carries 8 bytes and has its own hardware CRC, so none of the serial
// framing (start byte, version, length, CRC-8) is repeated. Multi-byte values are
// BIG-ENDIAN, like every temperature on the URTC bus.
//
//   SET_PREHEAT   host -> rack   ID RACK_CAN_ID_SET_PREHEAT, DLC 8
//     byte 0     tool ID (plain 5-bit value, see tool_id.h)
//     byte 1..2  target temperature in whole degrees C, big-endian
//     byte 3     sequence number (the same anti-replay counter as the serial frame)
//     byte 4..7  reserved, sent as 0, ignored on receipt
//
// The identifiers sit in a block that the URTC CAN documentation keeps for the rack.
#ifndef RACK_CAN_H
#define RACK_CAN_H

#include <stdint.h>

#include "rack_command.h"

#define RACK_CAN_ID_SET_PREHEAT 0x260u
#define RACK_CAN_ID_STATUS 0x261u  // rack -> host; reserved, the status frame is not defined yet
#define RACK_CAN_DLC 8u

// Writes the 8 data bytes of a SET_PREHEAT frame. Returns RACK_CAN_DLC.
uint8_t rack_can_encode_set_preheat(uint8_t seq, uint8_t tool_id, uint16_t target_temp_c, uint8_t out_data[8]);

// Decodes and range-checks a CAN frame as a SET_PREHEAT command, with the same rules as the
// serial command (rack_command_validate_set_preheat). A wrong identifier or a DLC other than
// 8 is RACK_CMD_ERR_WRONG_PAYLOAD_LEN. `out_command` and `out_seq` are only written on
// RACK_CMD_OK.
rack_command_status_t rack_can_decode_set_preheat(uint32_t can_id, const uint8_t *data, uint8_t dlc, uint8_t *out_seq, rack_set_preheat_t *out_command);

#endif // RACK_CAN_H
