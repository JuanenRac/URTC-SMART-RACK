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
//   REPLY         rack -> host   ID RACK_CAN_ID_REPLY, DLC 3
//     byte 0     the command that is answered (RACK_CMD_SET_PREHEAT)
//     byte 1     the tool byte that command carried (0xFF: it carried none)
//     byte 2     the reason, rack_reply_code_t (0 and 1 acknowledge, 2 to 8 refuse; the rack never sends 0xFF)
//   These are the same three bytes as the payload of the serial acknowledgement and refusal (rack_reply.h); on CAN the kind is told by the
//   reason, and there is no sequence number: the host matches the reply with its order by the command and the tool.
//
// The identifiers sit in the block 0x260..0x26F that the specification of URTC-COMMON (section "rack") keeps for the rack.
#ifndef RACK_CAN_H
#define RACK_CAN_H

#include <stdbool.h>
#include <stdint.h>

#include "rack_command.h"
#include "rack_link.h"

#define RACK_CAN_ID_SET_PREHEAT 0x260u
#define RACK_CAN_ID_REPLY 0x261u  // rack -> host: the reply to a command, an acknowledgement or a refusal with its reason
#define RACK_CAN_DLC 8u
#define RACK_CAN_REPLY_DLC 3u

typedef struct {
    bool ack;         // true for an acknowledgement (applied or repeat), false for a refusal
    uint8_t cmd;      // the command that was answered
    uint8_t tool_id;  // the tool byte of that command (0xFF: none)
    uint8_t code;     // rack_reply_code_t
} rack_can_reply_t;

// Writes the 8 data bytes of a SET_PREHEAT frame. Returns RACK_CAN_DLC.
uint8_t rack_can_encode_set_preheat(uint8_t seq, uint8_t tool_id, uint16_t target_temp_c, uint8_t out_data[8]);

// Decodes and range-checks a CAN frame as a SET_PREHEAT command, with the same rules as the
// serial command (rack_command_validate_set_preheat). A wrong identifier or a DLC other than
// 8 is RACK_CMD_ERR_WRONG_PAYLOAD_LEN. `out_command` and `out_seq` are only written on
// RACK_CMD_OK.
rack_command_status_t rack_can_decode_set_preheat(uint32_t can_id, const uint8_t *data, uint8_t dlc, uint8_t *out_seq, rack_set_preheat_t *out_command);

// Writes the 3 data bytes of the CAN reply that answers `decision` and returns RACK_CAN_REPLY_DLC, or 0 when nobody is to be told (RACK_REPLY_NONE or a code that does not
// exist, and a missing argument). Sent with the identifier RACK_CAN_ID_REPLY. Source only: there is no board with a CAN transceiver, so nothing calls it yet.
uint8_t rack_can_encode_reply(const rack_decision_t *decision, uint8_t out_data[RACK_CAN_REPLY_DLC]);

// The inverse, for the host side and the tests: true only for a well-formed reply (the identifier RACK_CAN_ID_REPLY, exactly 3 bytes and a reason that exists).
// `out` is only written on true.
bool rack_can_parse_reply(uint32_t can_id, const uint8_t *data, uint8_t dlc, rack_can_reply_t *out);

#endif // RACK_CAN_H
