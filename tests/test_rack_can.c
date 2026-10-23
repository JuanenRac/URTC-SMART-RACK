// =============================================================================
// URTC-SMART-RACK Firmware - tests for rack_can.c and the CAN path of rack_link.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// The command as it will look on the URTC bus: eight data bytes, big-endian temperature. No board
// carries it yet; these tests fix the byte order and the size before one does.
#include <string.h>

#include "test_runner.h"
#include "../src/preheat.h"
#include "../src/protocol.h"
#include "../src/rack_can.h"
#include "../src/rack_command.h"
#include "../src/rack_link.h"

void run_rack_can_tests(int *failures)
{
    uint8_t data[8];

    // The byte order is the one of every temperature on the URTC bus: big-endian.
    TEST_ASSERT(rack_can_encode_set_preheat(9, 0, 0x00C8u, data) == RACK_CAN_DLC, "a frame is eight bytes");
    TEST_ASSERT(data[0] == 0 && data[1] == 0x00 && data[2] == 0xC8 && data[3] == 9, "tool, temperature high byte first, sequence");
    TEST_ASSERT(data[4] == 0 && data[5] == 0 && data[6] == 0 && data[7] == 0, "the reserved bytes are sent as zero");
    TEST_ASSERT(rack_can_encode_set_preheat(0, 10, 0x0136u, data) == 8u && data[1] == 0x01 && data[2] == 0x36, "310 C is 0x0136, high byte first");
    TEST_ASSERT(rack_can_encode_set_preheat(0, 0, 0, NULL) == 0, "a missing buffer is refused");

    // Decoding gives back what was encoded, through the same validation as the serial command.
    uint8_t seq = 0;
    rack_set_preheat_t command = {0, 0};
    (void)rack_can_encode_set_preheat(42, 19, 350, data);
    TEST_ASSERT(rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT, data, 8, &seq, &command) == RACK_CMD_OK && seq == 42 && command.tool_id == 19 && command.target_temp_c == 350, "a hot-air head at 350 C is decoded");

    // A temperature in the old little-endian order is read as the number it really is, not as a valid one by luck: 200 C written that way is 0xC800.
    const uint8_t little_endian_by_mistake[8] = {0, 0xC8, 0x00, 1, 0, 0, 0, 0};
    TEST_ASSERT(rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT, little_endian_by_mistake, 8, &seq, &command) == RACK_CMD_ERR_TEMP_OUT_OF_RANGE, "200 C written little-endian is 51200 C and is refused");

    // Everything the serial command refuses, the CAN command refuses.
    (void)rack_can_encode_set_preheat(1, 0, 401, data);
    TEST_ASSERT(rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT, data, 8, &seq, &command) == RACK_CMD_ERR_TEMP_OUT_OF_RANGE, "above the rack ceiling is refused");
    (void)rack_can_encode_set_preheat(1, 5, 100, data);
    TEST_ASSERT(rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT, data, 8, &seq, &command) == RACK_CMD_ERR_TOOL_NOT_HEATABLE, "a tool without a heater is not heated");
    (void)rack_can_encode_set_preheat(1, 27, 0, data);
    TEST_ASSERT(rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT, data, 8, &seq, &command) == RACK_CMD_ERR_TOOL_ID_INVALID, "a tool id no profile uses is refused");
    (void)rack_can_encode_set_preheat(1, 0x40, 0, data);
    TEST_ASSERT(rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT, data, 8, &seq, &command) == RACK_CMD_ERR_TOOL_ID_INVALID, "a tool byte with the upper bits set is refused");

    // Wrong identifier, wrong length, missing arguments.
    (void)rack_can_encode_set_preheat(1, 0, 200, data);
    TEST_ASSERT(rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT + 1u, data, 8, &seq, &command) == RACK_CMD_ERR_WRONG_PAYLOAD_LEN, "another identifier is not a rack command");
    TEST_ASSERT(rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT, data, 7, &seq, &command) == RACK_CMD_ERR_WRONG_PAYLOAD_LEN, "seven bytes are not a rack command");
    TEST_ASSERT(rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT, data, 3, &seq, &command) == RACK_CMD_ERR_WRONG_PAYLOAD_LEN, "the three-byte serial payload is not a CAN frame");
    TEST_ASSERT(rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT, NULL, 8, &seq, &command) == RACK_CMD_ERR_WRONG_PAYLOAD_LEN, "a missing buffer is refused");

    // Nothing is written on a refusal.
    seq = 0xEE;
    command.tool_id = 0xEE;
    (void)rack_can_encode_set_preheat(1, 0, 999, data);
    (void)rack_can_decode_set_preheat(RACK_CAN_ID_SET_PREHEAT, data, 8, &seq, &command);
    TEST_ASSERT(seq == 0xEE && command.tool_id == 0xEE, "a refused frame leaves the outputs alone");

    // The decision path: same rules as the serial frame.
    link_watchdog_t lw;
    rack_decision_t d;
    link_watchdog_init(&lw, 1000);
    (void)rack_can_encode_set_preheat(3, 0, 200, data);
    rack_link_decide_can(&lw, RACK_CAN_ID_SET_PREHEAT, data, 8, 100, &d);
    TEST_ASSERT(d.action == RACK_ACT_SET_TARGET && d.tool_id == 0 && d.temp_c == 200 && d.acknowledge == 1, "a good CAN frame sets the target");
    TEST_ASSERT(!link_watchdog_is_link_lost(&lw, 150), "and proves the link alive");
    rack_link_decide_can(&lw, RACK_CAN_ID_SET_PREHEAT, data, 8, 110, &d);
    TEST_ASSERT(d.action == RACK_ACT_NONE && d.acknowledge == 1, "the same sequence again is a retransmission: acknowledged, nothing applied");
    (void)rack_can_encode_set_preheat(4, 0, 900, data);
    rack_link_decide_can(&lw, RACK_CAN_ID_SET_PREHEAT, data, 8, 120, &d);
    TEST_ASSERT(d.action == RACK_ACT_SAFE_STATE && d.tool_id == 0 && d.temp_c == preheat_safe_state_temp_c(), "an out-of-range CAN command puts its tool in the safe state");
    (void)rack_can_encode_set_preheat(5, 0x41, 900, data);
    rack_link_decide_can(&lw, RACK_CAN_ID_SET_PREHEAT, data, 8, 130, &d);
    TEST_ASSERT(d.action == RACK_ACT_NONE, "a command that names no tool changes nothing");

    // A frame that is not ours does not touch the watchdog.
    link_watchdog_t silent;
    link_watchdog_init(&silent, 1000);
    rack_link_decide_can(&silent, 0x123u, data, 8, 10, &d);
    TEST_ASSERT(d.action == RACK_ACT_NONE && link_watchdog_is_link_lost(&silent, 10), "a foreign identifier neither acts nor revives the watchdog");
    rack_link_decide_can(&silent, RACK_CAN_ID_SET_PREHEAT, data, 5, 10, &d);
    TEST_ASSERT(d.action == RACK_ACT_NONE && link_watchdog_is_link_lost(&silent, 10), "a short frame neither acts nor revives the watchdog");
    rack_link_decide_can(NULL, RACK_CAN_ID_SET_PREHEAT, data, 8, 10, &d);
    rack_link_decide_can(&silent, RACK_CAN_ID_SET_PREHEAT, NULL, 8, 10, &d);
    rack_link_decide_can(&silent, RACK_CAN_ID_SET_PREHEAT, data, 8, 10, NULL);
    TEST_ASSERT(d.action == RACK_ACT_NONE, "missing arguments change nothing");
}
