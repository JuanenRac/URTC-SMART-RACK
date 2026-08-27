// =============================================================================
// URTC-SMART-RACK Firmware - Shared types, defines, and version identity
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
// GPL-3.0 - see LICENSE
// =============================================================================
#ifndef FIRMWARE_COMMON_H
#define FIRMWARE_COMMON_H

#include <stdint.h>

// =============================================================================
// TARGET MCU: STM32G4 series (exact part TBD - no PCB/schematic exists yet
// for this board; see hardware/. Cortex-M4F is fixed across the whole G4 family, so
// this firmware and its linker script (STM32G4_MINIMAL.ld) target that core
// generically rather than guessing a specific flash/RAM size that a real
// schematic hasn't confirmed yet - see that file's own header comment for
// the placeholder memory map and what changes once real hardware exists.
//
// Sibling repo URTC (see URTC/src/F303-master/firmware_common.h) follows
// this exact same pattern for its own STM32F303 boards: version macros
// bumped in place by bump_version.py before every real build, odometer
// carry (PATCH+1, rolling into MINOR past 9). Reused here rather than
// reinvented.
// =============================================================================
#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_PATCH 3

// Encodes MAJOR.MINOR.PATCH as a single monotonically-increasing integer
// (major*10000 + minor*100 + patch) - cheap to compare or report over CAN
// later without formatting a string, same convention as URTC's own
// firmware/firmware_manifest.json "version_code" field.
#define FIRMWARE_VERSION_CODE \
    (FIRMWARE_VERSION_MAJOR * 10000u + FIRMWARE_VERSION_MINOR * 100u + FIRMWARE_VERSION_PATCH)

#endif // FIRMWARE_COMMON_H
