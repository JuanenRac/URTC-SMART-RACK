# Source map

Every file of the repository with a line on what it does: the annotated tree that the README carried before it was reduced to the first level (the first level, in eight languages, is in the section Repository Structure of the README).

```text
URTC-SMART-RACK/
├── src/                            # Firmware source
│   ├── firmware_common.h           # FIRMWARE_VERSION_MAJOR/MINOR/PATCH
│   ├── tool_id.h / .c              # Real: 5-bit raw reading -> tool ID decoding
│   ├── lifecycle.h / .c            # Real: usage-cycle/time tracking, maintenance-due check
│   ├── preheat.h / .c              # Real: Smart Idle activation + target temperature + safe-state target
│   ├── protocol.h / .c             # Real: versioned frame format + CRC8 parse/encode
│   ├── urtc_crc8.h                 # Real: CRC-8 shared with the vision sensor link (copied from URTC-COMMON)
│   ├── urtc_frame.h                # Real: frame layer (start byte, version, length, CRC) shared with the vision sensor link (copied from URTC-COMMON)
│   ├── urtc_rate_limiter.h         # Real: minimum-interval limiter of the commands of a tool (copied from URTC-COMMON)
│   ├── urtc_common.lock.json       # Lock of the files copied from URTC-COMMON
│   ├── rack_command.h / .c         # Real: command decode + actuation-limit validation
│   ├── link_watchdog.h / .c        # Real: link timeout + command idempotency
│   ├── rack_link.h / .c            # Real: frame-dispatch decision (rack_link_decide) tying protocol/rack_command/link_watchdog/preheat together
│   ├── rack_inventory.h / .c       # Real: multi-slot tool inventory - which tool is racked where, one tool never in two slots at once
│   ├── tool_limits.h / .c          # Real: highest pre-heat target per tool profile (0 = no heater)
│   ├── rack_app.h / .c             # Real: applies the receive-path decision to the inventory; safe state while the link is lost
│   ├── rack_deframer.h / .c        # Real: finds the frames in the byte stream of the serial link (resynchronisation, silence timeout)
│   ├── rack_can.h / .c             # Real: how the rack command will travel on CAN once a board with a transceiver exists
│   ├── preheat_limit.h / .c        # Real: the longest pre-heat without a tool swap
│   ├── preheat_plan.h / .c         # Real: predictive pre-heat from the job queue
│   ├── slot_verify.h / .c          # Real: what a thermal reading of a slot says about it
│   ├── rack_units.h / .c           # Real: identity and wear of each physical tool head
│   ├── rack_ring.h / .c            # Real: single-producer / single-consumer byte ring, the only thing a receive interrupt may touch
│   ├── rack_persist.h / .c         # Real: lifecycle and inventory in two CRC-checked copies that survive a power cut at any byte
│   ├── main.c                      # Minimal entry point (proof-of-life heartbeat loop)
│   ├── startup_stm32g4_minimal.c   # Vector table + Reset_Handler (no ST HAL yet, see file header)
│   └── STM32G4_MINIMAL.ld          # Placeholder linker script (128K FLASH / 32K RAM floor)
├── tests/                          # Real host-native test harness (tool_id, tool limits, lifecycle, preheat, protocol, rack_command, link_watchdog, rack link scenarios, rack inventory, robustness, rack_app) and Python checks that the build scripts and the published test count agree
├── docs/                           # Operations guide (docs/OPERATIONS.md): what the logic does, refuses and leaves unverified
├── hardware/                       # Hardware notes (README.md) - no design files yet (no schematic, no PCB)
├── firmware/                       # Build output (.bin/.elf/.hex), git-ignored - published only as assets of the GitHub release
├── build/                          # Intermediate build objects (gitignored)
├── images/                         # Media and diagrams
├── tools/
│   ├── build_test.py               # Non-versioning build/compile check
│   ├── gcov_floor.py               # Fails when the host tests run less of src/ than a floor (gcov)
│   ├── publish_release_assets.py   # Lists the release assets to attach and prints the gh command
│   ├── ci_validate.py              # Manifest/CHANGELOG/docs validation used by CI
│   └── _doc_policy.py / _readme_parity.py / _tracked_names.py # Shared checks used by ci_validate.py
├── bump.py                         # Explicit version bump command (building never bumps)
├── bump_version.py                 # version bump (generic, shared with URTC), called by bump.py
├── bump_manifest_version.py        # Syncs urtc.project.json's version to the native one (--sync)
├── build_firmware.sh / .bat        # Real build: host tests + compile + link + publish (never changes the version)
├── build-test.sh / .bat            # Non-versioning build/compile check
└── README.md
```
