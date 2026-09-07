// =============================================================================
// URTC-SMART-RACK Firmware - tests for lifecycle.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/lifecycle.h"

void run_lifecycle_tests(int *failures)
{
    lifecycle_t lc;
    lifecycle_init(&lc);
    TEST_ASSERT(lc.total_cycles == 0, "init starts at 0 cycles");
    TEST_ASSERT(lc.total_seconds == 0, "init starts at 0 seconds");

    lifecycle_record_use(&lc, 45);
    TEST_ASSERT(lc.total_cycles == 1, "one recorded use is one cycle");
    TEST_ASSERT(lc.total_seconds == 45, "duration accumulates");

    lifecycle_record_use(&lc, 30);
    TEST_ASSERT(lc.total_cycles == 2, "cycles keep accumulating");
    TEST_ASSERT(lc.total_seconds == 75, "seconds keep accumulating");

    TEST_ASSERT(lifecycle_needs_maintenance(&lc, 100) == false, "2 cycles is not due for maintenance at a 100-cycle threshold");
    TEST_ASSERT(lifecycle_needs_maintenance(&lc, 2) == true, "2 cycles is due at a 2-cycle threshold");
    TEST_ASSERT(lifecycle_needs_maintenance(&lc, 1) == true, "past the threshold still reports due");

    lifecycle_record_use(NULL, 10);
    TEST_ASSERT(1, "recording on a NULL pointer does not crash the caller");
}
