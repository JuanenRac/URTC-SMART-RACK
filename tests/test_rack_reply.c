// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_reply.c and the reasons of rack_link.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// Every way a command can end, played through the real receive decision, answered with the real encoder and read back with the real parser: what the sender is told
// must be the reason the decision had.
#include "test_runner.h"
#include "../src/link_watchdog.h"
#include "../src/protocol.h"
#include "../src/rack_can.h"
#include "../src/rack_command.h"
#include "../src/rack_link.h"
#include "../src/rack_reply.h"
#include "../src/tool_id.h"
#include "../src/tool_limits.h"

static rack_decision_t decide_raw(link_watchdog_t *lw, uint8_t seq, uint8_t cmd, const uint8_t *payload, uint8_t len, uint32_t now_ms)
{
    uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
    const uint8_t n = protocol_encode_frame(seq, cmd, payload, len, buf);
    rack_decision_t d;
    rack_link_decide(lw, buf, n, now_ms, &d);
    return d;
}

static rack_decision_t decide_preheat(link_watchdog_t *lw, uint8_t seq, uint8_t tool, uint16_t temp, uint32_t now_ms)
{
    const uint8_t payload[3] = {tool, (uint8_t)(temp >> 8), (uint8_t)(temp & 0xFFu)};
    return decide_raw(lw, seq, RACK_CMD_SET_PREHEAT, payload, 3, now_ms);
}

// The reply the decision produces, parsed back; false when nobody is told.
static bool reply_of(const rack_decision_t *d, rack_reply_t *out)
{
    uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
    const uint8_t n = rack_reply_encode(d, buf);
    if (n == 0u) {
        return false;
    }
    protocol_frame_t frame;
    return protocol_parse_frame(buf, n, &frame) == PROTOCOL_OK && rack_reply_parse(&frame, out);
}

void run_rack_reply_tests(int *failures)
{
    rack_reply_t r;

    // --- applied, and a repeat of what was applied ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        rack_decision_t d = decide_preheat(&lw, 5, TOOL_ID_SOLDERING_IRON, 200, 100);
        TEST_ASSERT(d.acknowledge == 1 && d.reply == RACK_REPLY_APPLIED, "an applied command is answered APPLIED");
        TEST_ASSERT(reply_of(&d, &r) && r.ack && r.code == RACK_REPLY_APPLIED && r.seq == 5 && r.cmd == RACK_CMD_SET_PREHEAT && r.tool_id == TOOL_ID_SOLDERING_IRON, "the acknowledgement names the sequence, the command and the tool");
        d = decide_preheat(&lw, 5, TOOL_ID_SOLDERING_IRON, 200, 150);
        TEST_ASSERT(d.acknowledge == 1 && d.reply == RACK_REPLY_REPEAT && d.action == RACK_ACT_NONE, "a retransmission is answered REPEAT and applies nothing");
        TEST_ASSERT(reply_of(&d, &r) && r.ack && r.code == RACK_REPLY_REPEAT && r.seq == 5, "REPEAT is an acknowledgement too, so the sender stops resending");
    }

    // --- every refusal says why ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        rack_decision_t d = decide_preheat(&lw, 1, TOOL_ID_SOLDERING_IRON, RACK_COMMAND_MAX_TEMP_C + 50u, 100);
        TEST_ASSERT(d.reply == RACK_REPLY_TEMP_OUT_OF_RANGE && d.acknowledge == 0, "a temperature above the ceiling is refused with TEMP_OUT_OF_RANGE");
        TEST_ASSERT(reply_of(&d, &r) && !r.ack && r.code == RACK_REPLY_TEMP_OUT_OF_RANGE && r.tool_id == TOOL_ID_SOLDERING_IRON, "the refusal is a NACK that names the tool");

        d = decide_preheat(&lw, 2, 7, 100, 200); // a gripper has no heater
        TEST_ASSERT(d.reply == RACK_REPLY_NOT_HEATABLE, "a target above zero for a tool without a heater is NOT_HEATABLE");

        d = decide_preheat(&lw, 3, 27, 100, 300); // assigned to no tool
        TEST_ASSERT(d.reply == RACK_REPLY_TOOL_INVALID && d.action == RACK_ACT_NONE, "an identifier that names no tool is TOOL_INVALID and changes nothing");
        TEST_ASSERT(reply_of(&d, &r) && r.tool_id == 27, "the reply carries the byte as it was sent");

        d = decide_preheat(&lw, 4, 0xE3u, 100, 400); // upper bits set
        TEST_ASSERT(d.reply == RACK_REPLY_TOOL_INVALID, "a tool byte with upper bits set is TOOL_INVALID");

        d = decide_preheat(&lw, 5, TOOL_ID_NONE, 100, 500);
        TEST_ASSERT(d.reply == RACK_REPLY_TOOL_ABSENT || d.reply == RACK_REPLY_TOOL_INVALID, "a slot that holds nothing (31) is refused with a reason");
        TEST_ASSERT(d.reply != RACK_REPLY_APPLIED && d.acknowledge == 0, "and never acknowledged");

        const uint8_t one_byte[1] = {TOOL_ID_SOLDERING_IRON};
        d = decide_raw(&lw, 6, RACK_CMD_SET_PREHEAT, one_byte, 1, 600);
        TEST_ASSERT(d.reply == RACK_REPLY_WRONG_LENGTH, "a payload of the wrong size is WRONG_LENGTH");
        d = decide_raw(&lw, 7, RACK_CMD_SET_PREHEAT, one_byte, 0, 700);
        TEST_ASSERT(d.reply == RACK_REPLY_WRONG_LENGTH && d.reply_tool == 0xFFu, "an empty payload is WRONG_LENGTH and names no tool");
        TEST_ASSERT(reply_of(&d, &r) && r.tool_id == 0xFFu, "the reply of a command without a tool byte carries 0xFF");
    }

    // --- a command the rack does not have, and a flood ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        const uint8_t payload[3] = {0, 0, 0};
        rack_decision_t d = decide_raw(&lw, 9, 0x7Eu, payload, 3, 100);
        TEST_ASSERT(d.reply == RACK_REPLY_UNKNOWN_COMMAND && d.action == RACK_ACT_NONE && d.acknowledge == 0, "an unknown command is answered UNKNOWN_COMMAND and changes nothing");
        TEST_ASSERT(reply_of(&d, &r) && !r.ack && r.cmd == 0x7Eu && r.seq == 9, "the reply says which command was not understood");

        link_watchdog_init(&lw, 1000);
        link_watchdog_set_command_interval(&lw, 50);
        d = decide_preheat(&lw, 1, TOOL_ID_SOLDERING_IRON, 200, 1000);
        TEST_ASSERT(d.reply == RACK_REPLY_APPLIED, "the first command is applied");
        d = decide_preheat(&lw, 2, TOOL_ID_SOLDERING_IRON, 210, 1010);
        TEST_ASSERT(d.reply == RACK_REPLY_HELD_RATE && d.action == RACK_ACT_NONE && d.acknowledge == 0, "a command too soon after the last one is HELD_RATE: not applied, not acknowledged, and told why");
        d = decide_preheat(&lw, 2, TOOL_ID_SOLDERING_IRON, 210, 1100);
        TEST_ASSERT(d.reply == RACK_REPLY_APPLIED, "sent again after the interval it is applied");
    }

    // --- no answer to noise or to what is not for this rack ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        const uint8_t payload[3] = {0, 0, 200};
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        const uint8_t n = protocol_encode_frame(1, RACK_CMD_SET_PREHEAT, payload, 3, buf);
        buf[n - 1] ^= 0xFFu;
        rack_decision_t d;
        rack_link_decide(&lw, buf, n, 100, &d);
        TEST_ASSERT(d.reply == RACK_REPLY_NONE && !reply_of(&d, &r), "a corrupt frame is not answered: noise must not provoke traffic");
        uint8_t can_data[8] = {0};
        rack_link_decide_can(&lw, 0x123u, can_data, 8, 200, &d);
        TEST_ASSERT(d.reply == RACK_REPLY_NONE && !reply_of(&d, &r), "a CAN frame with another identifier is not answered");
        TEST_ASSERT(rack_reply_encode(NULL, buf) == 0u && rack_reply_encode(&d, NULL) == 0u, "no decision or no buffer encodes nothing");
    }

    // --- the CAN path gives the same reasons ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t data[8];
        rack_decision_t d;
        (void)rack_can_encode_set_preheat(3, TOOL_ID_HOTAIR_REWORK, 300, data);
        rack_link_decide_can(&lw, RACK_CAN_ID_SET_PREHEAT, data, RACK_CAN_DLC, 100, &d);
        TEST_ASSERT(d.reply == RACK_REPLY_APPLIED && d.reply_seq == 3 && d.reply_tool == TOOL_ID_HOTAIR_REWORK, "a CAN command is applied and answered like a serial one");
        (void)rack_can_encode_set_preheat(4, TOOL_ID_HOTAIR_REWORK, 900, data);
        rack_link_decide_can(&lw, RACK_CAN_ID_SET_PREHEAT, data, RACK_CAN_DLC, 200, &d);
        TEST_ASSERT(d.reply == RACK_REPLY_TEMP_OUT_OF_RANGE && d.reply_seq == 4, "a CAN command above the ceiling is refused with the reason and its sequence");
    }

    // --- the parser refuses what is not a reply ---
    {
        protocol_frame_t f;
        f.version = PROTOCOL_VERSION;
        f.seq = 1;
        f.cmd = RACK_REPLY_CMD_ACK;
        f.len = RACK_REPLY_PAYLOAD_LEN;
        f.payload[0] = RACK_CMD_SET_PREHEAT;
        f.payload[1] = 0;
        f.payload[2] = RACK_REPLY_APPLIED;
        TEST_ASSERT(rack_reply_parse(&f, &r) && r.ack, "a well-formed acknowledgement is read");
        f.payload[2] = RACK_REPLY_TEMP_OUT_OF_RANGE;
        TEST_ASSERT(!rack_reply_parse(&f, &r), "an acknowledgement that carries a refusal code is not a reply");
        f.cmd = RACK_REPLY_CMD_NACK;
        TEST_ASSERT(rack_reply_parse(&f, &r) && !r.ack, "a refusal with a refusal code is read");
        f.payload[2] = RACK_REPLY_APPLIED;
        TEST_ASSERT(!rack_reply_parse(&f, &r), "a refusal that carries APPLIED is not a reply");
        f.payload[2] = 0x40;
        TEST_ASSERT(!rack_reply_parse(&f, &r), "a code that does not exist is not a reply");
        f.payload[2] = RACK_REPLY_HELD_RATE;
        f.len = RACK_REPLY_PAYLOAD_LEN + 1u;
        TEST_ASSERT(!rack_reply_parse(&f, &r), "a payload of another length is not a reply");
        f.len = RACK_REPLY_PAYLOAD_LEN;
        f.cmd = RACK_CMD_SET_PREHEAT;
        TEST_ASSERT(!rack_reply_parse(&f, &r) && !rack_reply_parse(NULL, &r) && !rack_reply_parse(&f, NULL), "another command, or nothing at all, is not a reply");
    }
}
