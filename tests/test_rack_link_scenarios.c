// =============================================================================
// URTC-SMART-RACK Firmware - Host-side peripheral/link scenarios
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// The review's own "simulador de periferico para probar comando
// invalido sin rack" / "Evidencia: ... trama valida, CRC invalido, timeout
// y comando fuera de rango, con transicion segura comprobada": this file
// plays real encoded frames (some deliberately corrupted, some
// deliberately out of range) through rack_link.c's own real
// rack_link_process_frame() (which itself ties together protocol.c/
// rack_command.c/link_watchdog.c/preheat.c) exactly as a real receive
// interrupt handler eventually will, and checks the real resulting
// actuation decision at each step - no rack, CAN transceiver or F-RAM
// required, since none exist for this board yet (see main.c's own note).
//
// Found while auditing the code: this real
// decision used to be a static function defined INSIDE this test file
// rather than a real src/ module anything else could call - promoted to
// rack_link.c/.h, with this file now calling INTO it instead of defining
// its own private copy of the logic under test.
#include "test_runner.h"
#include "../src/protocol.h"
#include "../src/rack_command.h"
#include "../src/link_watchdog.h"
#include "../src/preheat.h"
#include "../src/rack_link.h"
#include "../src/tool_id.h"

void run_rack_link_scenario_tests(int *failures)
{
    // --- Scenario 1: a real valid frame commands the real requested temp ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t payload[3] = {2, 0xC8, 0x00}; // tool_id=2, 200C
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(1, RACK_CMD_SET_PREHEAT, payload, 3, buf);

        uint16_t applied = rack_link_process_frame(&lw, buf, len, 100);
        TEST_ASSERT(applied == 200, "a real, valid, in-range frame results in the real requested target temp being applied");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 100) == false, "the link is alive right after a real accepted frame");
    }

    // --- Scenario 2: a real corrupted CRC falls back to the safe state ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t payload[3] = {2, 0xC8, 0x00};
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(1, RACK_CMD_SET_PREHEAT, payload, 3, buf);
        buf[len - 1] ^= 0xFFu; // real bit-level corruption of the CRC byte

        uint16_t applied = rack_link_process_frame(&lw, buf, len, 100);
        TEST_ASSERT(applied == preheat_safe_state_temp_c(), "a real CRC-corrupted frame results in the safe state, never the (unreadable) requested temp");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 100) == true, "a corrupted frame must not revive the link watchdog - it was never really received");
    }

    // --- Scenario 3: a real link timeout forces the safe state even though a valid command was accepted earlier ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t payload[3] = {2, 0xC8, 0x00}; // 200C
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(1, RACK_CMD_SET_PREHEAT, payload, 3, buf);
        uint16_t applied_at_start = rack_link_process_frame(&lw, buf, len, 0);
        TEST_ASSERT(applied_at_start == 200, "the real command is accepted and applied at t=0");

        // 2000ms pass with no further real frame arriving at all.
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 2000) == true, "1000ms past the last real frame, the link is lost");
        // A real caller (the eventual main loop) must re-check link
        // health independently of whatever the last accepted command
        // was - this asserts the real, honest transition a lost link
        // requires, not the (200C) value rack_link_process_frame() itself
        // returned at t=0 and never revisits on its own.
        uint16_t safe_target = link_watchdog_is_link_lost(&lw, 2000) ? preheat_safe_state_temp_c() : 200;
        TEST_ASSERT(safe_target == 0, "once the link is lost, the real applied target must fall back to the safe state (0), not the stale 200C");
    }

    // --- Scenario 4: a real, well-formed but out-of-range command falls back to the safe state ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint16_t too_hot = RACK_COMMAND_MAX_TEMP_C + 50u;
        uint8_t payload[3] = {2, (uint8_t)(too_hot & 0xFF), (uint8_t)(too_hot >> 8)};
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(1, RACK_CMD_SET_PREHEAT, payload, 3, buf);

        uint16_t applied = rack_link_process_frame(&lw, buf, len, 100);
        TEST_ASSERT(applied == preheat_safe_state_temp_c(), "a real, CRC-valid frame requesting an out-of-range temp still results in the safe state, not the dangerous requested value");
        // The frame itself was real and CRC-valid, so the *link* is
        // genuinely alive even though the *command* inside it was refused
        // - these are deliberately independent real signals.
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 100) == false, "the link stays alive on a valid-but-refused command - framing and command validity are independent checks");
    }

    // --- Scenario 5: a real duplicate (resent) sequence is not re-applied ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t payload[3] = {2, 0xC8, 0x00};
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(9, RACK_CMD_SET_PREHEAT, payload, 3, buf);

        uint16_t first = rack_link_process_frame(&lw, buf, len, 0);
        uint16_t resend = rack_link_process_frame(&lw, buf, len, 10);
        TEST_ASSERT(first == 200, "the first real delivery of sequence 9 is applied");
        TEST_ASSERT(resend == preheat_safe_state_temp_c(), "a real resend of the exact same sequence 9 is treated as a duplicate, not re-applied");
    }

    // --- a real link-loss recovery is a genuine epoch boundary -
    // rack_link_process_frame() must reset per-tool sequence tracking so
    // a tool board that itself rebooted (and restarted its own sequence
    // counter low) is never mistaken for replaying a stale old command. ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t payload[3] = {2, 0xC8, 0x00}; // 200C
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(200, RACK_CMD_SET_PREHEAT, payload, 3, buf);

        uint16_t applied_at_start = rack_link_process_frame(&lw, buf, len, 0);
        TEST_ASSERT(applied_at_start == 200, "sequence 200 is accepted and applied at t=0");

        // The link now goes silent for well past its 1000ms timeout - a
        // real disconnect (or the tool board itself rebooting).
        uint8_t low_seq_len = protocol_encode_frame(5, RACK_CMD_SET_PREHEAT, payload, 3, buf);
        uint16_t after_recovery = rack_link_process_frame(&lw, buf, low_seq_len, 5000);
        TEST_ASSERT(
            after_recovery == 200,
            "a real command with a LOW sequence number (5), arriving after the link was genuinely lost, is accepted as a fresh session - not rejected as 'behind' sequence 200 from before the outage"
        );
    }

    // --- a real, CRC-valid but zero-payload SET_PREHEAT frame must
    // never read payload[0] as a tool_id (there is no real payload byte to
    // read - see protocol.h's own comment on the smallest legal frame) and
    // must never consume/mutate ANY tool's watchdog sequence slot as a
    // side effect of trying anyway. tool_id_decode(0) == 0, and tool 0 is
    // a real, present tool slot per tool_id.h (only all-ones/0x1F means
    // "absent") - so this is a real, reachable false tool identity, not a
    // hypothetical one, and this scenario is what the old code (before
    // this fix) would have silently mistaken for a legitimate frame from
    // tool 0 once 's other fix (protocol.c zeroing the unused payload
    // tail) made payload[0] well-defined as 0 for this frame instead of
    // undefined stack garbage. ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(77, RACK_CMD_SET_PREHEAT, NULL, 0, buf);

        uint16_t applied = rack_link_process_frame(&lw, buf, len, 100);
        TEST_ASSERT(applied == preheat_safe_state_temp_c(), "a real, CRC-valid, zero-payload SET_PREHEAT frame results in the safe state, never a guessed temp");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 100) == false, "the frame was real and CRC-valid, so the link itself is still honestly alive");
        for (uint8_t tool = 0; tool < LINK_WATCHDOG_TOOL_SLOTS; tool++) {
            TEST_ASSERT(!lw.has_seq_by_tool_id[tool], "no tool's sequence slot (tool 0 included) was consumed by a frame with no real tool_id byte");
        }

        // A REAL subsequent frame for tool 0, with a sequence number lower
        // than the bogus 77 the zero-payload frame above carried, must
        // still be accepted - proving tool 0's tracking genuinely was
        // never touched, not merely that this particular check happened
        // to still pass.
        uint8_t payload[3] = {0, 0xC8, 0x00}; // tool_id=0, 200C
        uint8_t real_len = protocol_encode_frame(3, RACK_CMD_SET_PREHEAT, payload, 3, buf);
        uint16_t real_applied = rack_link_process_frame(&lw, buf, real_len, 200);
        TEST_ASSERT(real_applied == 200, "a real tool-0 command with a low sequence number (3) is accepted - the earlier zero-payload frame never poisoned tool 0's anti-replay state with its own sequence (77)");
    }

    // --- an unrelated/unsupported command must not read payload[0]
    // as a tool_id either, even with a non-zero payload - only
    // RACK_CMD_SET_PREHEAT's own wire layout defines byte 0 as a tool_id
    // today. ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t payload[1] = {0}; // would decode as tool 0 if ever (wrongly) read as a tool_id
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(9, 0x7F, payload, 1, buf); // 0x7F: not a real rack_command_id_t value

        uint16_t applied = rack_link_process_frame(&lw, buf, len, 100);
        TEST_ASSERT(applied == preheat_safe_state_temp_c(), "an unrecognized command results in the safe state");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 100) == false, "the link stays alive on a real, CRC-valid frame even carrying an unrecognized command");
        TEST_ASSERT(!lw.has_seq_by_tool_id[0], "an unrecognized command's own payload byte 0 is never mistaken for a tool_id");
    }

    // --- (watchdog sequence consumption, reviewed as the finding
    // asked): a real duplicate resend - correctly rejected so its command
    // is never re-applied - must still count as real evidence the link
    // itself is up, since the exact same bytes arriving twice is honest
    // proof the transport is working, not a sign of silence. ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        uint8_t payload[3] = {2, 0xC8, 0x00};
        uint8_t buf[PROTOCOL_MAX_FRAME_SIZE];
        uint8_t len = protocol_encode_frame(9, RACK_CMD_SET_PREHEAT, payload, 3, buf);

        rack_link_process_frame(&lw, buf, len, 0);
        // The link goes silent for nearly (but not quite) the full
        // timeout, then the exact same frame is resent - a real duplicate,
        // correctly rejected as stale/already-applied.
        uint16_t resend = rack_link_process_frame(&lw, buf, len, 900);
        TEST_ASSERT(resend == preheat_safe_state_temp_c(), "the resend is still correctly rejected as a duplicate, never re-applied");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 1899) == false, "a real (if duplicate) frame at t=900 keeps the link alive up to just before a fresh 1000ms timeout from THAT frame, not just from the original one at t=0");
    }
}
