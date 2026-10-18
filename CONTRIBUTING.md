# Contributing to URTC-SMART-RACK 🦾

We welcome contributions to the intelligent end-effector management system of the HYDRA-UMC platform.

## Technology Stack
- **Language**: C11.
- **Planned target (not confirmed)**: an STM32G4 microcontroller; the exact part is undecided, because no PCB or schematic exists yet.
- **Planned interfaces (not confirmed)**: CAN / FDCAN, SPI and I2C, plus current and temperature sensing. None of them has been chosen or tested yet - the logic in `src/` is host-tested pure C.

## Guidelines
1. **Thermal Safety**: Any changes to the pre-heating logic must include failsafe checks to prevent tool overheating or fire hazards.
2. **Deterministic Control**: The firmware must use non-blocking state machines for tool identification and thermal regulation.
3. **Link Consistency**: Rack frames follow the format in `docs/OPERATIONS.md` (`src/protocol.h`). That format is not a CAN frame yet; whoever maps it onto the bus must document the identifiers and byte order next to the CAN documentation of the URTC project.
4. **Hardware Validation**: Once design files exist in `hardware/`, test PCB modifications against them. Until then `hardware/README.md` says what is missing.
5. **Tests**: every change to `src/` comes with host tests in `tests/`, and `build_firmware.sh` / `build_firmware.bat` list the same sources (a test checks it).
