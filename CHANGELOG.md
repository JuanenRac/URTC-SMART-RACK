# Changelog

All notable work on **URTC-SMART-RACK** is summarized here, newest first.
This file intentionally omits calendar dates from individual entries.

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

## Unreleased - Promoted the real receive-path decision to a real src/ module

- **New `src/rack_link.h`/`.c`** (`rack_link_process_frame()`) - found while
  auditing the code: the real frame-dispatch
  decision tying `protocol.c`/`rack_command.c`/`link_watchdog.c`/
  `preheat.c` together existed only as a `static` function inside
  `tests/test_rack_link_scenarios.c`, not as a real `src/` module
  anything else could call. Promoted verbatim (same real logic, same
  behavior) so it is ready to wire into a real UART/CAN receive ISR the
  day the PCB exists - `tests/test_rack_link_scenarios.c` now calls INTO
  this module instead of defining its own private copy of the logic
  under test. `build_firmware.sh`/`.bat`'s host-test step compiles the
  new file alongside the existing ones.
  Verified for real: this session's shell again had no host `gcc`/`cc`
  on `PATH` - compiled and RAN the real host-side test suite (all 8 test
  files, all 7 `src/` modules including the new one) with the
  already-installed MSVC toolchain (`cl.exe`, VS2019 Build Tools) as a
  real substitute host compiler, same technique already used for this
  repo's own 0.0.7 entry below - `All tests passed.`, 0 failures.

## [0.0.8] - A stale/reordered sequence can no longer be re-applied as a fresh command (RACK-01)

Found while auditing the code:

- **The bug.** `link_watchdog_accept_sequence()` only ever rejected an
  EXACT repeat of the immediately-previous sequence number - the
  sequence 10, 11, 10 accepted the second 10 outright (10 != 11),
  re-applying a real stale/replayed command as if it were fresh. This
  was real idempotency-in-name-only, not the anti-reorder protection its
  own header comment implied.
- **The fix.** Treats the 8-bit sequence space as a signed ring: a
  command is only accepted if it is genuinely newer than the last one
  seen for that tool slot (a delta in `(0, 127]`, correctly handling
  real 8-bit wraparound like `254 -> 0`), never merely "different from
  the last one". New `link_watchdog_reset_all_sequences()` gives this a
  real, principled session/epoch boundary too: `rack_link_process_frame()`
  now calls it whenever the link was already considered lost before a
  frame arrives, so a tool board that itself rebooted (and restarted its
  own counter low) is never mistaken for replaying a stale command from
  before the outage. Still bounded, ordinary idempotency/anti-reorder
  protection for one continuous session - not cryptographic anti-replay,
  documented honestly as such.
- 9 new tests (5 in `test_link_watchdog.c` - the exact "10, 11, 10"
  reproduction, real wraparound accepted as forward progress *and*
  rejected once genuinely stale again, and the reset clearing every
  tracked tool slot; 1 new end-to-end scenario in
  `test_rack_link_scenarios.c` proving a low sequence number is accepted
  as a fresh session right after a real link-loss recovery).
- README (all 7 languages) also corrected: its own quoted host-test
  build command was missing `src/rack_link.c` from the compiled file
  list - copy-pasting it verbatim would have failed to link, since
  `test_rack_link_scenarios.c` calls into `rack_link_process_frame()`.

Verified for real: compiled and ran the exact host-test command
(`cl /std:c11 ...`, MSVC Build Tools as the substitute host compiler -
this session's shell had no `gcc`/`cc` on `PATH`, same real environment
condition and same substitute already used for this repo's own 0.0.7
entry) - `All tests passed.`, 0 failures, all 9 new assertions included.

## [0.0.7] - Real rack protocol: framing, CRC, command limits, timeout and idempotency

- **`src/protocol.h`/`.c`** (new) - the real wire framing for the host<->rack link: `[SOF][VERSION][SEQ][CMD][LEN][PAYLOAD][CRC8]`. `protocol_parse_frame()` never partially trusts a frame - bad SOF, an unsupported version, a length beyond `PROTOCOL_MAX_PAYLOAD`, an incomplete buffer, and a real corrupted CRC (byte-level or payload-level) are each their own distinct, real error status. `protocol_encode_frame()`/`protocol_crc8()` are the exact inverse, used by tests and the scenario harness below to build real frames rather than hand-encoding byte arrays.
- **`src/rack_command.h`/`.c`** (new) - real command validation, separate from the framing layer: `rack_command_validate_set_preheat()` decodes and range-checks a `RACK_CMD_SET_PREHEAT` payload against `RACK_COMMAND_MAX_TEMP_C` (400°C - above the hottest real tool type `preheat.c` already knows, 350°C for hot air, with a real margin) and a real, present tool ID (reusing `tool_id.h`, not re-deriving its own notion of "no tool"). A frame can be perfectly well-formed at the wire level and still ask for something this firmware must refuse - that judgment lives here, not in the framing parser.
- **`src/link_watchdog.h`/`.c`** (new) - real link timeout and command idempotency. `link_watchdog_is_link_lost()` is true both after a real timeout AND before the first frame has ever arrived (a link never proven alive is treated exactly like one that just died - real "estado seguro al arrancar"). `link_watchdog_accept_sequence()` rejects an exact sequence number already accepted for a given tool slot (a real link-retry resend), independently per tool.
- **`src/preheat.h`/`.c`** gained `preheat_safe_state_temp_c()` (always 0/off) - the real target a caller must fall back to once the link is lost or a command is refused, additive alongside the existing `preheat_target_temp_c()`/`preheat_should_activate()`.
- **49 new host-side test assertions** (`test_protocol.c`: 19, `test_rack_command.c`: 8, `test_link_watchdog.c`: 11) = 74 total with the existing 25, all passing. **`test_rack_link_scenarios.c`** (new, 11 assertions) is the real host-side peripheral/link simulator the diagnosis notes asked for: it plays real encoded frames - valid, CRC-corrupted, a well-formed but out-of-range command, a real elapsed-timeout, and a real duplicate resend - through `protocol.c`/`rack_command.c`/`link_watchdog.c`/`preheat.c` exactly as a future receive handler will, and checks the real resulting actuation decision (the requested temp, or the real safe state) at each step, without a rack, CAN transceiver or F-RAM, none of which exist for this board yet.
- `build_firmware.sh`/`.bat`'s host-test step now compiles and runs all 4 new test files alongside the existing 3.
- Real verification note: this session's shell had no host `gcc`/`cc` on `PATH` (a real environment condition, not a project defect - see the ecosystem's own prior note about Flutter/Go availability varying by terminal). Compiled and ran the exact same host-side test sources with the already-installed MSVC toolchain (`cl.exe`, VS2019 Build Tools) as a real substitute host compiler - `All tests passed.`, 0 failures. The `arm-none-eabi-gcc` cross-compile/link step (unaffected by this pass - `main.c` is untouched) was independently re-verified for real, producing `firmware/URTC_SMART_RACK_FIRMWARE_v0.0.7.{bin,elf,hex}`.
- Still out of scope, on purpose: wiring `protocol.c`/`rack_command.c`/`link_watchdog.c` into a real UART/CAN receive path in `main.c` - there is still no PCB/transceiver to receive real frames from.

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
