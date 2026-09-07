# Contributing to URTC-SMART-RACK 🦾

We welcome contributions to the intelligent end-effector management system of the HYDRA-UMC platform.

## Technology Stack
- **Language**: C11.
- **Hardware**: STM32G474 (Main Controller).
- **Protocols**: CAN / FDCAN, SPI, I2C.
- **Sensors**: Current sensing (ACS712), Temperature (NTC/Thermocouple).

## Guidelines
1. **Thermal Safety**: Any changes to the pre-heating logic must include failsafe checks to prevent tool overheating or fire hazards.
2. **Deterministic Control**: The firmware must use non-blocking state machines for tool identification and thermal regulation.
3. **CAN Consistency**: Ensure that all rack-specific CAN frames follow the `HYDRA-UMC` ecosystem's protocol documentation.
4. **Hardware Validation**: Test PCB modifications against the Eagle/Gerber files provided in the `hardware/` directory.
