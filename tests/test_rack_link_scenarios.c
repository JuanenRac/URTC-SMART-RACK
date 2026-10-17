// =============================================================================
// URTC-SMART-RACK Firmware - Host-side peripheral/link scenarios
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// Real encoded frames (some deliberately corrupted, some deliberately out of range) played through rack_link_decide(), the decision a real receive interrupt makes - no rack,
// CAN transceiver or F-RAM required, since none exist for this board yet (see main.c's own note).
//
// A frame that is not a command to act on now (noise, a retransmission, an unknown command) changes NOTHING; it used to come out as "0 degrees", which switched the heater off.
#include "test_runner.h"
#include "../src/protocol.h"
#include "../src/rack_command.h"
#include "../src/link_watchdog.h"
#include "../src/preheat.h"
#include "../src/rack_link.h"
#include "../src/tool_id.h"

static rack_decision_t decide(link_watchdog_t *lw, uint8_t seq, uint8_t tool, uint16_t temp, uint32_t now_ms)
{
    uint8_t payload[3] = {tool, (uint8_t)(temp & 0xFFu), (uint8_t)(temp >> 8)};
    uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
    uint8_t len = protocol_encode_frame(seq, RACK_CMD_SET_PREHEAT, payload, 3, buf);
    rack_decision_t decision;
    rack_link_decide(lw, buf, len, now_ms, &decision);
    return decision;
}

void run_rack_link_scenario_tests(int *failures)
{
    // --- Scenario 1: a valid frame asks for the requested temperature of its tool ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        rack_decision_t d = decide(&lw, 1, 2, 200, 100);
        TEST_ASSERT(d.action == RACK_ACT_SET_TARGET && d.tool_id == 2 && d.temp_c == 200 && d.acknowledge == 1, "a valid, in-range frame sets 200 C on tool 2 and is acknowledged");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 100) == false, "the link is alive right after a real accepted frame");
    }

    // --- Scenario 2: a corrupted CRC changes nothing (and does not revive the watchdog) ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t payload[3] = {2, 0xC8, 0x00};
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(1, RACK_CMD_SET_PREHEAT, payload, 3, buf);
        buf[len - 1] ^= 0xFFu;
        rack_decision_t d;
        rack_link_decide(&lw, buf, len, 100, &d);
        TEST_ASSERT(d.action == RACK_ACT_NONE && d.acknowledge == 0, "a CRC-corrupted frame is ignored: noise must not switch a heater off");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 100) == true, "a corrupted frame must not revive the link watchdog - it was never really received");
    }

    // --- Scenario 3: a link timeout is the caller's safe state, whatever was accepted earlier ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        rack_decision_t d = decide(&lw, 1, 2, 200, 0);
        TEST_ASSERT(d.action == RACK_ACT_SET_TARGET && d.temp_c == 200, "the command is accepted at t=0");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 2000) == true, "1000ms past the last real frame, the link is lost (the caller then brings every tool to the safe state)");
    }

    // --- Scenario 4: a well-formed but out-of-range command puts ITS tool in the safe state ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        rack_decision_t d = decide(&lw, 1, 2, RACK_COMMAND_MAX_TEMP_C + 50u, 100);
        TEST_ASSERT(d.action == RACK_ACT_SAFE_STATE && d.tool_id == 2 && d.temp_c == preheat_safe_state_temp_c(), "a CRC-valid frame requesting an out-of-range temperature puts tool 2 in the safe state, never the dangerous value");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 100) == false, "the link stays alive on a valid-but-refused command - framing and command validity are independent checks");
    }

    // --- Scenario 5: a retransmission is acknowledged and changes nothing (it used to switch the heater off) ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        rack_decision_t first = decide(&lw, 9, 2, 200, 0);
        rack_decision_t resend = decide(&lw, 9, 2, 200, 10);
        TEST_ASSERT(first.action == RACK_ACT_SET_TARGET && first.temp_c == 200, "the first delivery of sequence 9 is applied");
        TEST_ASSERT(resend.action == RACK_ACT_NONE && resend.acknowledge == 1, "a resend of the same sequence 9 changes nothing - the target stays 200 C - and is acknowledged so the sender can stop");
    }

    // --- Scenario 5b: two tools in the rack, interleaved frames: every decision names its tool ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        rack_decision_t a = decide(&lw, 1, 2, 200, 0);
        rack_decision_t b = decide(&lw, 1, 3, 300, 10);
        rack_decision_t a_again = decide(&lw, 1, 2, 200, 20); // retransmission for tool 2
        rack_decision_t a_next = decide(&lw, 2, 2, 210, 30);
        TEST_ASSERT(a.action == RACK_ACT_SET_TARGET && a.tool_id == 2 && a.temp_c == 200, "tool 2 gets 200 C");
        TEST_ASSERT(b.action == RACK_ACT_SET_TARGET && b.tool_id == 3 && b.temp_c == 300, "tool 3 gets 300 C, and the caller knows it is tool 3's");
        TEST_ASSERT(a_again.action == RACK_ACT_NONE, "a retransmission for tool 2 does not touch tool 3 or switch tool 2 off");
        TEST_ASSERT(a_next.action == RACK_ACT_SET_TARGET && a_next.tool_id == 2 && a_next.temp_c == 210, "the next command for tool 2 is applied");
    }

    // --- Scenario 5c: a refused command does not spend the sequence number of the good one that follows ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        rack_decision_t bad = decide(&lw, 9, 2, RACK_COMMAND_MAX_TEMP_C + 1u, 0);
        rack_decision_t good = decide(&lw, 9, 2, 200, 10);
        TEST_ASSERT(bad.action == RACK_ACT_SAFE_STATE, "the out-of-range command is refused");
        TEST_ASSERT(good.action == RACK_ACT_SET_TARGET && good.temp_c == 200, "the valid command with the same sequence number is applied, not thrown away as a duplicate of the refused one");
    }

    // --- A link-loss recovery is a genuine epoch boundary: per-tool sequence tracking is reset ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        rack_decision_t first = decide(&lw, 200, 2, 200, 0);
        TEST_ASSERT(first.action == RACK_ACT_SET_TARGET, "sequence 200 is accepted at t=0");
        rack_decision_t after = decide(&lw, 5, 2, 200, 5000);
        TEST_ASSERT(after.action == RACK_ACT_SET_TARGET && after.temp_c == 200, "a LOW sequence number (5) arriving after the link was lost is a fresh session, not 'behind' sequence 200 from before the outage");
    }

    // --- A CRC-valid zero-payload SET_PREHEAT frame never reads payload[0] as a tool_id and never consumes any tool's sequence slot ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(77, RACK_CMD_SET_PREHEAT, NULL, 0, buf);
        rack_decision_t d;
        rack_link_decide(&lw, buf, len, 100, &d);
        TEST_ASSERT(d.action == RACK_ACT_NONE, "a zero-payload SET_PREHEAT frame changes nothing, and guesses no temperature or tool");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 100) == false, "the frame was real and CRC-valid, so the link itself is honestly alive");
        for (uint8_t tool = 0; tool < LINK_WATCHDOG_TOOL_SLOTS; tool++) {
            TEST_ASSERT(!lw.has_seq_by_tool_id[tool], "no tool's sequence slot was consumed by a frame with no tool_id byte");
        }
        rack_decision_t real = decide(&lw, 3, 0, 200, 200);
        TEST_ASSERT(real.action == RACK_ACT_SET_TARGET && real.tool_id == 0 && real.temp_c == 200, "a real tool-0 command with a low sequence number (3) is accepted: the zero-payload frame never poisoned tool 0's anti-replay state");
    }

    // --- An unrecognized command changes nothing and never reads payload[0] as a tool_id ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t payload[1] = {0};
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(9, 0x7F, payload, 1, buf); // 0x7F: not a real rack_command_id_t value
        rack_decision_t d;
        rack_link_decide(&lw, buf, len, 100, &d);
        TEST_ASSERT(d.action == RACK_ACT_NONE, "an unrecognized command changes nothing");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 100) == false, "the link stays alive on a real, CRC-valid frame even carrying an unrecognized command");
        TEST_ASSERT(!lw.has_seq_by_tool_id[0], "an unrecognized command's own payload byte 0 is never mistaken for a tool_id");
    }

    // --- A resend is real evidence the link is up: it keeps the watchdog alive from THAT frame on ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        decide(&lw, 9, 2, 200, 0);
        rack_decision_t resend = decide(&lw, 9, 2, 200, 900);
        TEST_ASSERT(resend.action == RACK_ACT_NONE, "the resend changes nothing");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 1899) == false, "a real (if duplicate) frame at t=900 keeps the link alive up to just before a fresh 1000ms timeout from THAT frame");
    }
}
