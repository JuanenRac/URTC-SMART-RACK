// =============================================================================
// URTC-SMART-RACK Firmware - tests for link_watchdog.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/link_watchdog.h"
#include "../src/tool_id.h"

void run_link_watchdog_tests(int *failures)
{
    // --- Real "safe state at boot": link never proven alive counts as lost ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 0) == true, "a freshly initialized watchdog (no frame ever received) is lost, even at t=0");
    }

    // --- Real timeout math ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        link_watchdog_note_frame_received(&lw, 5000);
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 5999) == false, "999ms after the last real frame, with a 1000ms timeout, the link is still alive");
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 6000) == true, "exactly at the 1000ms timeout boundary, the link is lost");
    }

    // --- Real recovery: a fresh frame revives a lost link ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        link_watchdog_note_frame_received(&lw, 0);
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 2000) == true, "the link is lost 2000ms after the only frame, with a 1000ms timeout");
        link_watchdog_note_frame_received(&lw, 2000);
        TEST_ASSERT(link_watchdog_is_link_lost(&lw, 2500) == false, "a real new frame revives the link immediately");
    }

    // --- Real idempotency: an exact sequence repeat is rejected ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 3, 10) == true, "the first real sequence seen for a tool slot is accepted");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 3, 10) == false, "the exact same sequence resent for the same tool slot is rejected as a duplicate");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 3, 11) == true, "a genuinely new sequence for the same tool slot is still accepted");
    }

    // --- Real per-tool independence ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 1, 5) == true, "tool 1's sequence 5 is accepted");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 2, 5) == true, "the same sequence number 5 for a DIFFERENT tool slot is independently accepted");
    }

    // --- Real absent tool ID is never tracked, always rejected ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, TOOL_ID_NONE, 1) == false, "a command sequence for TOOL_ID_NONE (no tool present) is never accepted");
    }

    // --- RACK-01 (found in an ecosystem-wide software-improvements
    // audit, P1): a stale/reordered sequence arriving AFTER a newer one
    // must be rejected, not just an exact repeat of the immediately-
    // previous value. This is the finding's own exact reproduction. ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 4, 10) == true, "sequence 10 is accepted first");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 4, 11) == true, "sequence 11 (genuinely newer) is accepted");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 4, 10) == false, "sequence 10 arriving AFTER 11 is a stale/reordered replay, not a fresh command - the exact bug this audit found");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 4, 12) == true, "a genuinely newer sequence after the rejected replay is still accepted normally");
    }

    // --- RACK-01: real 8-bit sequence wraparound is still accepted as forward progress ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 5, 254) == true, "sequence 254 accepted");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 5, 255) == true, "sequence 255 accepted");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 5, 0) == true, "a real 8-bit wraparound (255 -> 0) is still forward progress, not a stale replay");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 5, 1) == true, "sequence 1 right after the wrap is accepted normally");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 5, 255) == false, "255 arriving again AFTER the wrap to 0/1 is a real stale replay of an old value, not accepted just because the byte is numerically larger");
    }

    // --- RACK-01: link_watchdog_reset_all_sequences() clears every tracked slot ---
    {
        link_watchdog_t lw;
        link_watchdog_init(&lw, 1000);
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 6, 200) == true, "tool 6 sequence 200 accepted");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 7, 50) == true, "tool 7 sequence 50 accepted");
        link_watchdog_reset_all_sequences(&lw);
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 6, 5) == true, "after a real reset (a new session/epoch), a low sequence number for tool 6 is accepted again, not rejected as 'behind' the old session's 200");
        TEST_ASSERT(link_watchdog_accept_sequence(&lw, 7, 1) == true, "the reset applies to every tracked tool slot, not just the one that triggered it");
    }
}
