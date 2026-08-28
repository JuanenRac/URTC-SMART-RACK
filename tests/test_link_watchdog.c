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
}
