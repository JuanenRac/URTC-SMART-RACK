// =============================================================================
// URTC-SMART-RACK Firmware - Answers to the host: rack_reply.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The frame that tells the host what became of a command: an acknowledgement when it was applied (or had already been), a refusal with the reason when it was not. Same
// framing as every other frame of the link (protocol.h), so a host that reads frames at all can read it; a host that has never heard of it simply ignores a command it does
// not know, so nothing that already works changes.
//
//   cmd     RACK_REPLY_CMD_ACK (0x81) or RACK_REPLY_CMD_NACK (0x82)
//   seq     the sequence number of the frame being answered
//   payload [0] the command being answered   [1] the tool byte it carried (0xFF: none)   [2] rack_reply_code_t
//
// Pure byte-buffer logic like protocol.c: nothing here touches a UART or a CAN peripheral.
#ifndef RACK_REPLY_H
#define RACK_REPLY_H

#include <stdbool.h>
#include <stdint.h>

#include "protocol.h"
#include "rack_link.h"

#define RACK_REPLY_CMD_ACK 0x81u
#define RACK_REPLY_CMD_NACK 0x82u
#define RACK_REPLY_PAYLOAD_LEN 3u

typedef struct {
    bool ack;         // true for an acknowledgement (applied or repeat), false for a refusal
    uint8_t cmd;      // the command that was answered
    uint8_t tool_id;  // the tool byte of that command
    uint8_t code;     // rack_reply_code_t
    uint8_t seq;      // the sequence number of the frame that was answered
} rack_reply_t;

// Writes the frame that answers `decision` into `out_buf` (room for PROTOCOL_MAX_FRAME_SIZE bytes) and returns its length, or 0 when nobody is to be told
// (RACK_REPLY_NONE, or a decision without a reply).
uint8_t rack_reply_encode(const rack_decision_t *decision, uint8_t out_buf[PROTOCOL_MAX_FRAME_SIZE]);

// The inverse, for the host side and the tests: true only for a well-formed reply (the right command, the exact payload length and a code that exists).
bool rack_reply_parse(const protocol_frame_t *frame, rack_reply_t *out);

#endif // RACK_REPLY_H
