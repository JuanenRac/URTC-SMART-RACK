# Changelog

All notable work on **URTC-SMART-RACK** is summarized here, newest first.
Full session-by-session detail (including dates) lives in a private,
unpublished internal log - this file is public, so it intentionally omits
calendar dates.

## Versioning scheme

`src/firmware_common.h`'s `FIRMWARE_VERSION_MAJOR`/`_MINOR`/`_PATCH` bump
automatically on every real build (`build_firmware.sh` / `.bat` - see
`bump_version.py`, run as the first real step of both scripts, the same
generic script sibling repo URTC uses). It follows the ecosystem-wide
base-10 "odometer" rule rather than semantic-versioning judgment calls:

- `PATCH` +1 on every build
- when `PATCH` would exceed 9, it resets to 0 and `MINOR` +1 instead (e.g. `0.0.9` -> `0.1.0`, never `0.0.10`)
- the same carry cascades into `MAJOR` if `MINOR` would exceed 9

---

## [0.0.6] - Real v0: hardware-independent tool logic (ID decode, lifecycle, pre-heat)

- **`src/tool_id.h`/`.c`** - decodes a raw 5-bit ID-jumper/F-RAM reading into a tool identity (`tool_id_decode()`), with `TOOL_ID_NONE` (all bits set) reserved as "no tool present". Pure bit masking, no GPIO/F-RAM driver needed.
- **`src/lifecycle.h`/`.c`** - the README's "Lifecycle Logs" feature: `lifecycle_t` (cycle + usage-time counters), `lifecycle_record_use()`, `lifecycle_needs_maintenance()` against a caller-supplied threshold.
- **`src/preheat.h`/`.c`** - the README's "Smart Idle" workflow: `preheat_target_temp_c()` (200°C for a soldering iron, matching the README's own diagram; 350°C for hot air; 0 for tools with nothing to preheat) and `preheat_should_activate()` (starts pre-heating once an anticipated swap is within its lead time, not before and not after it already happened).
- **`tests/`** - a minimal, dependency-free host-side test harness (`test_runner.h`'s `TEST_ASSERT`, `test_main.c`) compiled with the *host's* C compiler, never `arm-none-eabi-gcc` - these are pure-logic tests, not anything that touches real MCU registers. 25 real assertions across `test_tool_id.c`, `test_lifecycle.c`, `test_preheat.c`.
- **`build_firmware.sh`/`.bat`** - new step 2, before the version bump and the ARM cross-compile: builds and runs the host-native test suite, failing the whole build if any assertion fails.
- Still out of scope: real GPIO/F-RAM/CAN drivers to actually read a tool's ID, persist its lifecycle counters, and command a real heater - all need the PCB this board doesn't have yet.

## [0.0.3]

- Build version synchronized with `hydra-umc.project.json` and the repository-native version source.

## [0.0.0] - Initial scaffolding

- **`src/firmware_common.h`** - version identity (`FIRMWARE_VERSION_MAJOR/MINOR/PATCH`).
  No pinout/hardware ID defined yet - there is no PCB for this board.
- **`src/startup_stm32g4_minimal.c`** - hand-written Cortex-M4F vector
  table and `Reset_Handler` (`.data`/`.bss` init, hand off to `main()`),
  standing in for ST's own CMSIS/HAL startup until a real PCB pins down
  the exact STM32G4 part.
- **`src/STM32G4_MINIMAL.ld`** - placeholder linker script (128K FLASH /
  32K RAM, the smallest floor a mainstream STM32G4 part ships with).
- **`src/main.c`** - minimal proof-of-life entry point (heartbeat counter).
- **`bump_version.py`** - generic odometer-style version bump script,
  identical to sibling repo URTC's own (copied rather than reinvented).
- **`build_firmware.sh` / `build_firmware.bat`** - real build: version
  bump, cross-compile with `arm-none-eabi-gcc` for Cortex-M4F, link,
  `objcopy` to `.bin`/`.hex`, size report, publish versioned artifacts to
  `firmware/`.
- The real tool-tracking, pre-heat, and CAN integration features
  described in the README are the next milestone - they all need a real
  PCB first (F-RAM, ID jumpers, CAN transceiver, thermal sensor wiring).
