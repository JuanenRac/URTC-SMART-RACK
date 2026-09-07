// =============================================================================
// URTC-SMART-RACK Firmware - Minimal host-side test harness: test_runner.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// No external test framework dependency, on purpose: these tests compile
// and run with the *host's* plain gcc/cc, never arm-none-eabi-gcc - they
// exercise pure logic (tool_id.c, lifecycle.c, preheat.c), not anything
// that touches real MCU registers, so a real embedded toolchain isn't
// needed to run them.
#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include <stdio.h>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("  FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); \
            (*failures)++; \
        } \
    } while (0)

void run_tool_id_tests(int *failures);
void run_lifecycle_tests(int *failures);
void run_preheat_tests(int *failures);
void run_protocol_tests(int *failures);
void run_rack_command_tests(int *failures);
void run_link_watchdog_tests(int *failures);
void run_rack_link_scenario_tests(int *failures);

#endif // TEST_RUNNER_H
