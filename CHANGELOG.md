# Changelog

All notable changes to this project are documented here.

## [Unreleased]

- **The README follows the universal layout in the eight languages:** the sections are in the fixed order with the fixed names, the workflow diagram is drawn from `docs/architecture.json` (the Mermaid source stays in a collapsible block), the Repository Structure comes from `docs/structure.json`, and the sections for role, security, interfaces, testing and roadmap are new. The build text now says that building never changes the version.
- **The layout of the README is checked in the CI:** the validator runs `tools/universal_readme_check.py` and lists, as a warning, what differs from the universal layout of the README.
- **Project status is the second section of the README,** right after At a glance, in the eight languages.
- **The full CI runs every night:** a scheduled workflow runs the complete baseline again on the default branch and opens (or updates) one issue when it fails, so a change in a runner image, a toolchain or a pinned dependency is found the next day instead of with the next unrelated change.
- **The receive path and the memory of the rack have their pure logic:** `rack_ring.c` is a single-producer / single-consumer byte ring (the only thing a receive interrupt may touch, so the watchdog and the deframer stay in one context), and `rack_persist.c` keeps the lifecycle and the inventory in two copies with a magic, a structure version, a sequence number and a CRC-8, published by writing the magic last, so a power cut at any byte of a save leaves the old state or the new one and never a mixture. Both are host-tested (with the sanitizers and a store that loses power after every possible byte) and compile for the target; the memory chip behind the storage is not chosen yet, so only two callbacks are missing. The test count is current (531 checks in 18 files).
- **One list of host test sources:** `tools/host_sources.txt` is read by both build scripts, so a source or a test file is added in one place and the Linux and Windows builds can no longer differ.

## [0.2.1] - The serial line is held to shared streams of bytes and a limit on how often a tool can be commanded

- **The serial line is held to shared streams of bytes:** `tests/vectors/rack_vectors.h` (written by URTC-COMMON) now carries 19 streams, each a list of bursts of bytes with the time they arrived, the frames the deframer must hand over and the six counters it must end with (frames, CRC errors, bad headers, resyncs, timeouts, dropped bytes). `test_rack_vectors.c` feeds them byte by byte to the real `rack_deframer_push` / `rack_deframer_next` and compares frames and counters: a frame in pieces and split at every byte, noise before and between frames, a silence of exactly the timeout (kept) and one millisecond more (dropped), a wrap of the millisecond clock inside a frame, a bad CRC, a version that does not fit, a length of 17, a header that hides the start of a real frame, thirty start bytes in a row, a faster 5 ms timeout. The model of URTC-COMMON (`StreamDeframer`, written from the same description) and the firmware agree on all of them. 3 more assertions (467).
- **A limit on how often a tool can be commanded:** the rack holds back a pre-heat command for a tool when it accepted another for the same tool less than 20 ms before (`RACK_COMMAND_MIN_INTERVAL_MS`, a build option; `link_watchdog_set_command_interval()` takes the value at run time and 0 switches the limit off). It uses the minimum-interval limiter of URTC-COMMON (`urtc_rate_limiter.h`, copied with the lock file). A held-back command changes nothing, is not acknowledged and does not spend its sequence number, so the sender's next frame after the interval is obeyed; it still counts as proof that the link is alive. A refused value (the safe state) and the retransmission of a command already applied are never held back, and each tool is counted on its own. The Web Studio rack bench has the option, with the same default.
- **The README files carry sections written once in URTC-COMMON:** "Compatibility", in the eight languages, between marker comments before the license heading (`tools/readme_sections.py` of URTC-COMMON writes and checks them; the rest of each README is hand-written as before).
- **A static check on every push:** `ruff.toml` (unused names, exception chaining and syntax errors, the rules the flasher and the tester already ran) and a step of the CI that runs `ruff check .` with a pinned ruff; nothing to fix.
- **The frame layer comes from URTC-COMMON:** `protocol_parse_frame` and `protocol_encode_frame` use `src/urtc_frame.h` (copied with `vendor_copy.py --set firmware_frame --only urtc_frame.h`) instead of their own checks of the start byte, the version, the length and the CRC, shared with the vision sensor link; the order of the checks and every status are unchanged (a compile-time check holds the order of the statuses) and the host tests, the rack vectors and the parse vectors pass with the sanitizers. The rack keeps no rate limiter of its own: the link watchdog and the anti-replay sequence are what bound the commands it accepts.
- The `TEST_ASSERT` macro of the host tests comes from URTC-COMMON (`tests/vectors/urtc_test.h`, copied with `vendor_copy.py --set firmware_test`) instead of being written again in `tests/test_runner.h`; the suites are unchanged.
- **The host tests are held to a coverage floor:** the CI builds them with gcov counters (`URTC_COVERAGE_FLOOR` in `build_firmware.sh`) and `tools/gcov_floor.py` fails the build when they execute less than 90 percent of the lines of `src/` (98 percent today).
- **The serial frame is big-endian, version 2:** the temperature of `SET_PREHEAT` was the one multi-byte value of the family that travelled little-endian. `PROTOCOL_VERSION` is now 2 and `rack_command_validate_set_preheat()` reads the two temperature bytes big-endian, so the serial payload and the first three data bytes of the CAN form (`rack_can.c`) are the same bytes and go through the same validation without being reordered. A frame of version 1 is refused as an unsupported version instead of being read as another temperature. The tests, the serial payload builders and the vectors copied from URTC-COMMON follow (the frames, their checksums and the link scenarios are new), and `docs/OPERATIONS.md` and the README of every language say so.

## [0.2.0] - Rack frames use the CRC-8 shared with the vision link and checked against the shared vectors

- **One checksum for the rack and the vision link:** `protocol_crc8()` now calls `urtc_crc8()` of `src/urtc_crc8.h`, the header that URTC-COMMON keeps for the repositories that frame their data with CRC-8 (copied with `vendor_copy.py`, locked in `src/urtc_common.lock.json`); the test checks it against the check values URTC-COMMON computes on its own, among them the CRC-8 check value `0xF4` of the text `123456789`. The host tests run 441 assertions.

## [0.1.9] - Head units, thermal check of a slot and predictive pre-heat

- **Each head is a unit**: `rack_inventory.c` can key a slot by the type plus the serial number of the head (`rack_inventory_set_slot_unit()`), and `rack_units.c` keeps the use, the heater time and the thermal faults of each physical head, taken from its own counters (`0x1AA` / `0x1AB`), counting only the growth since the last reading and never a counter that went down.
- **Thermal check of a slot** (`slot_verify.c`): what a thermal reading of a slot says about the pre-heat that was ordered (reached, heating, too slow, overshoot, still hot, a heater on in an empty slot, no reading). The limits are estimates for a soldering tip.
- **Predictive pre-heat** (`preheat_plan.c`): turns the job queue into pre-heat orders using an estimate of the heating time of each tool plus a margin; switched off by default; it only suggests orders, which go through the normal command path and its limits.
- The rack model of URTC-COMMON (`conformance/rack_vectors.json`, `tests/vectors/rack_vectors.h`) is reproduced by the real code of the rack: frames, command checks, CAN data, link scenarios and thermal verdicts.
- The host tests run 439 assertions (new suites for the vectors and for the heads, the thermal check and the predictive pre-heat).
- The README lists URTC-GATEWAY, the new gateway in front of the CAN bus, among the related projects.

## [0.1.8] - A serial receive path, the CAN route, a limit on pre-heat and a CI that also builds on Windows

- `tools/ci_validate.py` fails when a versioned file has the name of a Windows path (a colon, the character a file system uses to store one, or a first folder named like a drive or a user profile): a path handed to WSL without translation once put build output inside a repository. The same check (`tools/_tracked_names.py`) is in every URTC repository.
- The `build-test` scripts use the interpreter named by `URTC_PYTHON` (which URTC-UPDATER sets to its own) and, without it, the first `python3` or `py -3` found, so a build check runs with a Python whose packages are known.
- **Serial receive path**: `rack_deframer.c` turns the byte stream of the serial link into frames. It looks for the start byte, collects the bytes the header announces, hands over a frame only when its CRC is right, resynchronises from the next start byte it already holds after a bad candidate (a byte lost inside a frame costs that frame, not the next one) and drops a half frame after an inter-byte timeout (20 ms by default, across the wrap of the clock). `rack_link_rx_byte()` and `rack_link_rx_next()` go from received bytes to the decision about the heater, and the deframer counts good frames, CRC errors, bad headers, resynchronisations, timeouts and dropped bytes.
- **CAN route, decided before the board**: `rack_can.c` fixes how the command will look on the URTC bus when a board with a transceiver exists - identifier `0x260`, eight data bytes, the temperature big-endian like every temperature on that bus, no repeated CRC - and `rack_link_decide_can()` applies the same validation, anti-replay and safe-state rules as the serial path. Nothing carries it yet; `docs/OPERATIONS.md` has the table.
- **Longest pre-heat without a tool swap**: `preheat_limit.c` ends the pre-heat of a slot after 10 minutes (build option `RACK_PREHEAT_MAX_MS`, run-time value of `preheat_limit_init()`, `PREHEAT_LIMIT_OFF` to disable) even though the host link is alive. Resending or raising the target does not restart the clock; after the limit the slot is latched until the host sends a target of 0 or the tool in the slot is replaced. `rack_app_apply_decision_limited()` and `rack_app_tick_limited()` apply it; the old functions keep working with no limit.
- The host tests run 365 assertions (new suites for the deframer, the CAN mapping and the pre-heat limit, run under the address and undefined-behaviour sanitizers in CI).
- The CI workflow also builds and tests on Windows (the `.bat` script with the ARM toolchain) and keeps the firmware images of both builds as workflow artifacts; its actions are pinned to a commit.
- The dashboard notice is sent only after the CI of the push finished green.
- The banner follows the animated style of the ecosystem banners, with the name, the kind and the technologies of the project.
- The manifest declares how far the project has been shown to work (`evidence`: host-test) and `tools/ci_validate.py` checks the value.
- The manifest notes are shorter.
- The CI workflow no longer carries the steps for languages the repository does not use, and installs the newlib packages the firmware build needs.
- The README of each language lists URTC-UPDATER among the related projects.

## [0.1.7] - An explicit bump command: building never changes the version

- Building no longer changes the firmware version: build_firmware.sh and build_firmware.bat only read it from src/firmware_common.h, and raising it is the explicit `python bump.py` step.
- Every licence statement and source header says GPL-3.0-or-later.

## [0.1.6] - Tool IDs, per-tool limits, safe link handling and builds that undo their version bump

- The tool ID space follows the URTC tool heads: 0 to 24 are tool profiles, 25 to 30 are refused, 31 (free configuration on a head, and this firmware's marker for an empty slot) is never heated, and a tool byte with any upper bit set is corruption and is refused instead of being read as another tool. A command that names nobody now changes nothing. The polarity of the ID bits is documented.
- New per-tool pre-heat limits (`tool_limits`): the 3D printer hotend is held to 310 C, a tool without a heater accepts only a target of 0, the soldering iron and hot-air head keep the 400 C ceiling, and the inventory enforces the same limit itself instead of trusting the caller.
- New `rack_app` module connects the decision of the receive path to the inventory: a target lands on the slot that holds the named tool, a refused command puts only that tool in the safe state, and a tick keeps every slot in the safe state while the link is lost or was never proven alive.
- The link watchdog replaces a timeout of 0 by 250 ms (it used to make every resend look new), latches a loss so that a silence of more than 49 days no longer looks like a living link when the millisecond counter wraps, and sizes its table from the tool ID width. The inventory refuses missing pointers and unknown tools, lifecycle counters stop at their maximum instead of wrapping to zero, and no C library function is needed any more.
- The firmware image carries its version word in a dedicated section that the linker keeps, right after the vector table; start-up enables the FPU before the first floating-point instruction and records the context of a hard fault in RAM that survives a reset.
- Build scripts: they work from any directory; every module is compiled for the Cortex-M4F target with function sections and conversion warnings, and all of them are linked together as a check; the Windows script honours URTC_CI and no longer raises the version in a validation build; a build that fails after the version was raised puts version, manifest and changelog back; CI runs the host tests with AddressSanitizer and UBSan and no longer runs twice for a pull request from the same repository.
- tools/build_test.py selects the WSL distribution with URTC_WSL_DISTRO (default distribution otherwise) and says clearly when WSL or the ARM toolchain is missing. The CRC routine no longer raises conversion warnings.
- Documentation: hardware/README.md says what is undecided, the operations guide covers the tool ID space, the per-tool limits, the link frame (not a CAN frame yet), the execution context and the new modules, CONTRIBUTING lists the target and interfaces as planned and not confirmed, SECURITY and the issue form point to this repository's own policy, and the README (all eight languages) no longer claims that the firmware version of each head is read.
- Published numbers: the README, the manifest and the tests agree on 290 test assertions, and a new check (tests/test_project_consistency.py) fails when the number, the host test sources of the two build scripts or the list of modules drift apart.

## [0.1.5] - Claims aligned with what has been tested

- Maturity is functional: nothing here has been verified on real hardware yet.
- The README (all eight languages) no longer shows an FDCAN badge (no CAN driver exists), renames the protocol range checks from safety limits, and says that the CAN integration has never run because no driver or board exists.
- The manifest notes count 138 host-native test assertions, as the README does.

## [0.1.4] - Firmware published only as release assets

- Firmware binaries (bin, elf, hex) are no longer versioned in git; they are published only as assets of the GitHub release of each version. build_firmware.sh and build_firmware.bat write them to firmware/ (git-ignored) in release mode and to build/firmware in CI mode.
- New tools/publish_release_assets.py checks the files in firmware/, writes the missing sha256 sidecar and prints the exact gh release create command with the files to attach; tests/test_publish_release_assets.py covers it. The READMEs (8 languages) describe the new flow.
- Verified: unit tests, the repository validation and a full ARM build in release mode. No behaviour of the firmware itself changed.

## [0.1.3] - Firmware version boundaries and side-effect-free CI builds

- The firmware version scripts keep their own three-component scheme and no longer hand over to the shared manifest utility, which broke the build at the boundary versions; the successor rule is now a pure function covered by boundary tests that run in CI and as a step of the build script.
- CI builds (URTC_CI=1) publish firmware images under build/firmware instead of firmware/, so a validation build leaves the tracked images untouched.
- Verified: unit tests and shell syntax checks only; the ARM firmware build itself was not run.

## [0.1.2] - A frame that is not a command changes nothing, and every decision names its tool

- **`rack_link_decide()` replaces `rack_link_process_frame()`.** The old function returned one number that meant two things - "apply this temperature" and "this frame does not count" - so noise on the CRC, an honest retransmission with the same sequence number or a command other than SET_PREHEAT came out as 0, which is "switch the heater off"; with two tools in the rack it also did not say which tool the number was for. The new function returns a decision (`NONE`, `SET_TARGET` or `SAFE_STATE`) that always names the tool: a frame that is not a command to act on changes nothing, a retransmission is acknowledged and changes nothing, and a command with an out-of-range value puts that tool in the safe state. Only a lost link (the watchdog) brings every tool to the safe state.
- **A refused command no longer spends the sequence number of the good one that follows it:** the command is checked before its sequence number is accepted.
- **The Windows build compiles and links** (`build_firmware.bat` left out the inventory module and its test, so the build failed from the start).
- `docs/OPERATIONS.md` says what the rack really does with a frame that is not a command; the scenarios cover two tools with interleaved frames and a refused command followed by a good one. Tests: all pass.

## [0.1.1]

- **Inventory reconciliation against slot-presence sensors
  (`rack_inventory_reconcile()`).** Run at boot and whenever the sensors
  change: a slot the inventory thinks is occupied but whose sensor reads
  empty is cleared, pre-heat target included (a tool pulled while the rack
  was off must never keep a heater command alive); a slot whose sensor
  reads occupied while the inventory holds nothing is reported for
  identification and stays unoccupied. 7 new assertions in
  `tests/test_rack_inventory.c` (138 in total). Wiring real sensors still
  needs the PCB. READMEs in all 7 languages and `docs/OPERATIONS.md`
  updated.

## [0.1.0]

- **Real multi-slot tool inventory (`rack_inventory.c`/`.h`).** Every
  other module tracked one tool_id at a time - a real Smart Rack (this
  project's own name, and its own README's "tracks... every URTC head")
  needs to know which tool is racked in which slot at once. New,
  independent, pure-C module: `rack_inventory_set_slot_tool()` racks a
  tool_id in a bounded slot (clearing TOOL_ID_NONE, matching this
  project's existing wire convention), `rack_inventory_find_tool()`
  answers "is this tool racked, and where", and
  `rack_inventory_set_preheat_target()` sets a per-slot pre-heat target
  reached by tool identity, refusing a target for a tool that isn't
  racked anywhere right now. Enforces the real physical invariant a rack
  needs: the same tool_id can never occupy two slots at once - re-racking
  a tool into a new slot clears its old one automatically instead of
  claiming it's in both places. 27 new `TEST_ASSERT` checks
  (`tests/test_rack_inventory.c`), 131 total host-native assertions now
  passing. `RACK_INVENTORY_MAX_SLOTS` (8) is a placeholder capacity, not
  a claim about real hardware - no PCB/schematic exists for this board
  yet, same honest scope as every other module here.

## [0.0.9] - A malformed zero-payload frame could no longer corrupt an unrelated tool's anti-replay state

- **New `src/rack_link.h`/`.c`** (`rack_link_process_frame()`) - found on code review: the real frame-dispatch
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
- **The bug.** `rack_link_process_frame()` read `frame.payload[0]`
  as a tool_id before ever checking that `frame.len` actually claims that
  byte exists. `protocol_parse_frame()` only ever copies `len` real wire
  bytes into `payload` - a real, validly-framed zero-payload frame
  (`len == 0`, the smallest legal frame per `protocol.h`'s own framing
  comment) left `payload[0]` as whatever undefined stack garbage the
  caller's own local happened to hold. That undefined "tool_id" then had
  its anti-replay sequence slot silently consumed by
  `link_watchdog_accept_sequence()` using this unrelated frame's own
  `seq` byte - real memory-unsafe behavior, and a real path to corrupting
  a genuine tool's sequence tracking purely from a malformed frame that
  happens to decode to that tool's ID.
- **The fix.** Two layers: `protocol_parse_frame()` now zero-fills the
  unused tail of `payload` past `len`, so any future reader gets a
  well-defined `0` instead of undefined memory; `rack_link_process_frame()`
  now checks `frame.len < 1` (and that `frame.cmd` is actually
  `RACK_CMD_SET_PREHEAT`, the only command whose wire layout defines
  byte 0 as a tool_id today) before ever decoding `payload[0]`, for any
  unrelated/unsupported command too.
- **Reviewed sequence consumption, as the finding also asked.** Marking
  the link alive (`link_watchdog_note_frame_received()`) used to be
  gated behind a successful anti-replay check, so a real, correctly
  rejected duplicate resend did not count as evidence the link was up -
  a tool board that only ever resends its last command (e.g. waiting for
  an ack that never existed) could have been spuriously declared "lost".
  Any real, CRC-valid frame now marks the link alive regardless of which
  command it carries or whether its own sequence gets re-applied - those
  are independent real signals.
- 11 new tests (`test_protocol.c`, `test_rack_link_scenarios.c`) -
  confirmed to fail against the pre-fix code (7 real failures reproduced
  via a local revert of just the two fixed files, tests kept), all pass
  with the fix.

## [0.0.8] - A stale/reordered sequence can no longer be re-applied as a fresh command

Found on code review:

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

- Build version synchronized with `urtc.project.json` and the repository-native version source.

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
- **`bump_version.py`** - generic version bump script,
  identical to sibling repo URTC's own (copied rather than reinvented).
- **`build_firmware.sh` / `build_firmware.bat`** - real build: version
  bump, cross-compile with `arm-none-eabi-gcc` for Cortex-M4F, link,
  `objcopy` to `.bin`/`.hex`, size report, publish versioned artifacts to
  `firmware/`.
- The real tool-tracking, pre-heat, and CAN integration features
  described in the README are the next milestone - they all need a real
  PCB first (F-RAM, ID jumpers, CAN transceiver, thermal sensor wiring).
