// =============================================================================
// URTC-SMART-RACK Firmware - Host-side test entry point: test_main.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#include "test_runner.h"

int main(void)
{
    int failures = 0;

    run_tool_id_tests(&failures);
    run_lifecycle_tests(&failures);
    run_preheat_tests(&failures);

    if (failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }
    printf("%d test(s) failed.\n", failures);
    return 1;
}
