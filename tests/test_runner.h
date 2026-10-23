// =============================================================================
// URTC-SMART-RACK Firmware - Minimal host-side test harness: test_runner.h
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// No external test framework dependency, on purpose: these tests compile
// and run with the *host's* plain gcc/cc, never arm-none-eabi-gcc - they
// exercise pure logic (tool_id.c, lifecycle.c, preheat.c), not anything
// that touches real MCU registers, so a real embedded toolchain isn't
// needed to run them.
#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

// TEST_ASSERT comes from URTC-COMMON (vendor_copy.py --set firmware_test).
#include "vectors/urtc_test.h"

void run_tool_id_tests(int *failures);
void run_lifecycle_tests(int *failures);
void run_preheat_tests(int *failures);
void run_protocol_tests(int *failures);
void run_rack_command_tests(int *failures);
void run_link_watchdog_tests(int *failures);
void run_rack_link_scenario_tests(int *failures);
void run_rack_inventory_tests(int *failures);
void run_robustness_tests(int *failures);
void run_rack_app_tests(int *failures);
void run_rack_deframer_tests(int *failures);
void run_rack_can_tests(int *failures);
void run_preheat_limit_tests(int *failures);
void run_rack_vectors_tests(int *failures);
void run_rack_units_tests(int *failures);

#endif // TEST_RUNNER_H
