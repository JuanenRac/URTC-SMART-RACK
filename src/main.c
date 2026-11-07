// =============================================================================
// URTC-SMART-RACK Firmware - Application entry point: main.c
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0-or-later - see LICENSE
// =============================================================================
// STARTING POINT ONLY - proves the ARM Cortex-M4F toolchain (same
// arm-none-eabi-gcc used by sibling repo URTC/src/F303-master) actually
// compiles and links a real firmware image for this board, not the tool
// tracking/pre-heat logic described in the README yet. There is no real
// PCB/schematic for URTC-SMART-RACK to date (see hardware/), so this
// firmware has nothing to drive - no F-RAM, no CAN transceiver, no ID
// jumpers wired up. That real work lands once hardware exists.
#include "firmware_common.h"
#include "protocol.h"
#include "rack_can.h"
#include "rack_main_loop.h"
#include "rack_reply.h"

// Read by a debugger/flasher/bootloader straight from the image, without a CAN
// link (this board has no confirmed CAN wiring yet - see firmware_common.h).
// Nothing in the program reads it, so `volatile` alone would not keep it: the
// linker (--gc-sections) discards unreferenced sections. `used` keeps the
// symbol through the compiler and the dedicated `.fw_version` section is KEPT
// by the linker script, right after the vector table, so the value sits at a
// fixed place in the image.
__attribute__((used, section(".fw_version")))
const uint32_t g_firmware_version_code = FIRMWARE_VERSION_CODE;

// Free-running counter incremented in the main loop below - the simplest
// possible proof-of-life a debugger or future watchdog can observe
// without any peripheral driver existing yet.
static volatile uint32_t g_heartbeat;

// The board supplies these three. Until a board exists, the weak defaults keep the image linkable (a clock that never advances and no transmitter), so
// the loop runs its logic with an idle link and holds every slot in the safe state. A port to the real PCB replaces them.
__attribute__((weak)) uint32_t rack_hal_millis(void)
{
    return 0u;
}

__attribute__((weak)) void rack_hal_serial_tx(const uint8_t *bytes, uint8_t length)
{
    (void)bytes;
    (void)length;
}

__attribute__((weak)) void rack_hal_can_tx(uint32_t can_id, const uint8_t *data, uint8_t length)
{
    (void)can_id;
    (void)data;
    (void)length;
}

static rack_ctx_t g_rack;

static void send_reply(const rack_decision_t *decision, bool from_can, void *user)
{
    (void)user;
    if (from_can) {
        uint8_t data[RACK_CAN_REPLY_DLC];
        const uint8_t length = rack_can_encode_reply(decision, data);
        if (length != 0u) {
            rack_hal_can_tx(RACK_CAN_ID_REPLY, data, length);
        }
    } else {
        uint8_t frame[PROTOCOL_MAX_FRAME_SIZE];
        const uint8_t length = rack_reply_encode(decision, frame);
        if (length != 0u) {
            rack_hal_serial_tx(frame, length);
        }
    }
}

int main(void)
{
    rack_main_init(&g_rack);
    for (;;) {
        g_heartbeat++;
        (void)rack_main_step(&g_rack, rack_hal_millis(), send_reply, 0);
    }
}
