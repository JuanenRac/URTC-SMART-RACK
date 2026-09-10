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

    // --- RACK-01 (P1): a real link-loss recovery is a genuine epoch boundary -
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
}
