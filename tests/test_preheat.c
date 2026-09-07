// =============================================================================
// URTC-SMART-RACK Firmware - tests for preheat.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "test_runner.h"
#include "../src/preheat.h"

void run_preheat_tests(int *failures)
{
    TEST_ASSERT(preheat_target_temp_c(TOOL_TYPE_SOLDERING_IRON) == 200, "soldering iron targets 200 C, per the README's own workflow diagram");
    TEST_ASSERT(preheat_target_temp_c(TOOL_TYPE_HOT_AIR) == 350, "hot air targets 350 C");
    TEST_ASSERT(preheat_target_temp_c(TOOL_TYPE_GENERIC) == 0, "generic tools have nothing to preheat");

    TEST_ASSERT(preheat_should_activate(5000, 10000) == true, "a swap 5s away with a 10s lead time should preheat now");
    TEST_ASSERT(preheat_should_activate(15000, 10000) == false, "a swap 15s away with a 10s lead time is still too far out");
    TEST_ASSERT(preheat_should_activate(10000, 10000) == true, "exactly at the lead time boundary still activates");
    TEST_ASSERT(preheat_should_activate(0, 10000) == false, "0ms until use means the swap already happened, not a future one to preheat for");
}
