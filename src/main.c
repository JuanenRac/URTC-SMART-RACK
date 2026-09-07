// =============================================================================
// URTC-SMART-RACK Firmware - Application entry point: main.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
// STARTING POINT ONLY - proves the ARM Cortex-M4F toolchain (same
// arm-none-eabi-gcc used by sibling repo URTC/src/F303-master) actually
// compiles and links a real firmware image for this board, not the tool
// tracking/pre-heat logic described in the README yet. There is no real
// PCB/schematic for URTC-SMART-RACK to date (see hardware/), so this
// firmware has nothing to drive - no F-RAM, no CAN transceiver, no ID
// jumpers wired up. That real work lands once hardware exists.
#include "firmware_common.h"

// Read by a debugger/reset inspection without needing a CAN link (this
// board has no confirmed CAN wiring yet - see firmware_common.h) - kept
// `volatile` so the compiler can never optimize it away even though
// nothing in this minimal image reads it back yet.
volatile uint32_t g_firmware_version_code = FIRMWARE_VERSION_CODE;

// Free-running counter incremented in the main loop below - the simplest
// possible proof-of-life a debugger or future watchdog can observe
// without any peripheral driver existing yet.
static volatile uint32_t g_heartbeat;

int main(void)
{
    for (;;) {
        g_heartbeat++;
    }
}
