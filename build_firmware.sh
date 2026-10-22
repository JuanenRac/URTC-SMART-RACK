#!/usr/bin/env bash
# HYDRA_UMC_SCRIPT_STANDARD_HEADER_BEGIN
# *****************************************************************************
# Project   : URTC-SMART-RACK
# Script    : build_firmware.sh
# Purpose   : Incremental firmware build and versioned artifact packaging workflow.
# Author    : JuanenRac (Electro Hobby 3D)
# Email     : electrohobby3d@gmail.com
# Copyright : (C) 2026 JuanenRac
# License   : GPL-3.0-or-later - see LICENSE
# *****************************************************************************
# HYDRA_UMC_SCRIPT_STANDARD_HEADER_END
# HYDRA_UMC_SCRIPT_STANDARD_BANNER_BEGIN
printf '\n*******************************************************************************\n'
printf '%s\n' "* URTC-SMART-RACK - build_firmware.sh"
printf '%s\n' "* Mode      : BUILD (no version change)"
printf '%s\n' "* Author    : JuanenRac (Electro Hobby 3D)"
printf '%s\n' "* Email     : electrohobby3d@gmail.com"
printf '%s\n' "* Copyright : (C) 2026 JuanenRac"
printf '%s\n' "* License   : GPL-3.0-or-later - see LICENSE"
printf '%s\n' "* ------------------------------------------------------------------------- *"
printf '%s\n' "* 1. Read the project version (compiling never changes it; run bump.py to raise it)."
printf '%s\n' "* 2. Run this project's declared build, verification and packaging commands."
printf '%s\n' "* 3. Report the result and keep an interactive terminal open."
printf '%s\n' "*******************************************************************************"
printf '\n'
# HYDRA_UMC_SCRIPT_STANDARD_BANNER_END
# URTC-SMART-RACK - Firmware Build Script: build_firmware.sh
# Compiles this project's minimal Cortex-M4F skeleton (src/main.c +
# src/startup_stm32g4_minimal.c, see those files' own header comments for
# why there's no ST HAL/CMSIS dependency yet - no real PCB exists to pin
# down the exact STM32G4 part). Same arm-none-eabi-gcc toolchain as sibling
# repo URTC/build_firmware.sh, reused rather than reinvented, but with the
# HAL/CMSIS fetch steps skipped entirely - there's no chip-specific vendor
# code to fetch yet at this project's current andamiaje (scaffolding) stage.
#
# Usage:
#   chmod +x build_firmware.sh   (one-time)
#   ./build_firmware.sh
set -e
URTC_CI_MODE="${URTC_CI:-0}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Every path below is relative to the repository root, whatever the directory the script is started from.
cd "$ROOT"
BUILD="$ROOT/build"
FIRMWARE_OUT="$ROOT/firmware"
if [ "$URTC_CI_MODE" = "1" ]; then
    echo "URTC-SMART-RACK CI: version sources are read-only."
    # Release mode writes the images to firmware/ (git-ignored, published only
    # as assets of the GitHub release - see tools/publish_release_assets.py);
    # CI mode writes them under build/ and touches no tracked file.
    FIRMWARE_OUT="$BUILD/firmware"
else
    # HYDRA_UMC_SCRIPT_STANDARD_VERSION_STEP
    printf '%s\n' "[1/3] Reading the project version (compiling never changes it; raise it with bump.py first)..."
    # HYDRA_UMC_SCRIPT_STANDARD_VERSION_CAPTURE_BEFORE
    HYDRA_UMC_VERSION_BEFORE="$(python3 -c 'import json, pathlib, sys; print(json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))["version"])' "$ROOT/urtc.project.json")"
fi

# The files a version bump rewrites. They are copied aside before the bump and
# put back if anything fails afterwards, so a build that does not produce an
# image never leaves a newer version, manifest or changelog behind.
VERSION_FILES="src/firmware_common.h urtc.project.json CHANGELOG.md"
VERSION_BACKUP="$BUILD/version_backup"
VERSION_BUMPED=0
on_exit() {
    status=$?
    if [ "$VERSION_BUMPED" = "1" ] && [ "$status" -ne 0 ]; then
        for f in $VERSION_FILES; do
            cp -f "$VERSION_BACKUP/$f" "$ROOT/$f"
        done
        echo "Build failed: version, manifest and changelog restored to their previous state."
    fi
    if [ -t 0 ]; then
        echo ""
        read -r -p "Press Enter to close this window..." _ || true
    fi
}
trap on_exit EXIT

echo ""
echo "=== 1. Toolchain ==="
if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "FAIL: arm-none-eabi-gcc not found. Install the ARM GNU Toolchain"
    echo "      (e.g. 'choco install gcc-arm-embedded' on Windows, or"
    echo "      'apt install gcc-arm-none-eabi' on Debian/Ubuntu), then re-run."
    exit 1
fi
echo "  OK   arm-none-eabi-gcc found: $(arm-none-eabi-gcc --version | head -1)"

echo ""
echo "=== 2. Real logic tests (host gcc, not arm-none-eabi-gcc) ==="
HOST_CC="${CC:-cc}"
if ! command -v "$HOST_CC" >/dev/null 2>&1; then
    HOST_CC=gcc
fi
if ! command -v "$HOST_CC" >/dev/null 2>&1; then
    echo "FAIL: no host C compiler found (tried \$CC and gcc)."
    exit 1
fi
mkdir -p "$BUILD"
# URTC_SANITIZE=1 (set by CI) builds the host tests with AddressSanitizer and UBSan.
SANITIZE_FLAGS=""
if [ "${URTC_SANITIZE:-0}" = "1" ]; then
    SANITIZE_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=undefined"
fi
"$HOST_CC" -std=c11 -Wall -Wextra -Wconversion -Isrc -Itests $SANITIZE_FLAGS \
    -o "$BUILD/host_tests" \
    tests/test_main.c tests/test_tool_id.c tests/test_lifecycle.c tests/test_preheat.c \
    tests/test_protocol.c tests/test_rack_command.c tests/test_link_watchdog.c tests/test_rack_link_scenarios.c \
    tests/test_rack_inventory.c tests/test_robustness.c tests/test_rack_app.c \
    tests/test_rack_deframer.c tests/test_rack_can.c tests/test_preheat_limit.c tests/test_rack_vectors.c tests/test_rack_units.c \
    src/tool_id.c src/tool_limits.c src/lifecycle.c src/preheat.c src/protocol.c src/rack_command.c src/link_watchdog.c src/rack_link.c \
    src/rack_inventory.c src/rack_app.c src/rack_deframer.c src/rack_can.c src/preheat_limit.c src/slot_verify.c src/rack_units.c src/preheat_plan.c
"$BUILD/host_tests"
echo "  OK   tool_id.c / tool_limits.c / lifecycle.c / preheat.c / protocol.c / rack_command.c / link_watchdog.c / rack_link.c / rack_inventory.c / rack_deframer.c / rack_can.c / preheat_limit.c / slot_verify.c / rack_units.c / preheat_plan.c pure-logic tests passed"

echo ""
echo "=== 2b. Build tooling tests (version odometer boundaries) ==="
python3 -m unittest discover -s tests -p "test_*.py"
echo "  OK   build tooling tests passed"

echo ""
echo "=== 3. Firmware version (read from the header; bump.py raises it, building never does) ==="
# Compiling never changes the version: it is read from the header. Raise it first
# with "python3 bump.py".
VERSION="$(grep -oE 'define[[:space:]]+FIRMWARE_VERSION_MAJOR[[:space:]]+[0-9]+' src/firmware_common.h | grep -oE '[0-9]+$').$(grep -oE 'define[[:space:]]+FIRMWARE_VERSION_MINOR[[:space:]]+[0-9]+' src/firmware_common.h | grep -oE '[0-9]+$').$(grep -oE 'define[[:space:]]+FIRMWARE_VERSION_PATCH[[:space:]]+[0-9]+' src/firmware_common.h | grep -oE '[0-9]+$')"
echo "  Firmware version: $VERSION"

echo ""
echo "=== 4. Compile + link ==="
mkdir -p "$BUILD" "$FIRMWARE_OUT"

CFLAGS="-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -Wall -Wextra -Wconversion -O2 -g -Isrc"
LDFLAGS="-T src/STM32G4_MINIMAL.ld -nostdlib -Wl,--gc-sections -Wl,-Map=$BUILD/urtc-smart-rack.map"

# Every module is compiled for the Cortex-M4F target, not only the two that the
# image links today: a warning or a missing C-library function in a logic module
# shows up now and not on the day the board exists.
mkdir -p "$BUILD/arm"
for source in src/*.c; do
    arm-none-eabi-gcc $CFLAGS -c "$source" -o "$BUILD/arm/$(basename "$source" .c).o"
done
echo "  OK   every src/*.c compiled for Cortex-M4F"

arm-none-eabi-gcc $CFLAGS $LDFLAGS "$BUILD/arm/startup_stm32g4_minimal.o" "$BUILD/arm/main.o" -o "$BUILD/urtc-smart-rack.elf"
echo "  OK   Linked $BUILD/urtc-smart-rack.elf"

# All modules must also resolve against each other without a C library; this
# link is only a check and produces no image.
arm-none-eabi-gcc $CFLAGS -T src/STM32G4_MINIMAL.ld -nostdlib -Wl,--gc-sections "$BUILD"/arm/*.o -o "$BUILD/all-modules-check.elf"
echo "  OK   all modules link together without a C library"

arm-none-eabi-objcopy -O binary "$BUILD/urtc-smart-rack.elf" "$BUILD/urtc-smart-rack.bin"
arm-none-eabi-objcopy -O ihex "$BUILD/urtc-smart-rack.elf" "$BUILD/urtc-smart-rack.hex"
echo "  OK   $BUILD/urtc-smart-rack.bin / .hex"

echo ""
echo "=== 5. Size report ==="
arm-none-eabi-size "$BUILD/urtc-smart-rack.elf"

echo ""
echo "=== 6. Write versioned artifacts to firmware/ (git-ignored) ==="
cp "$BUILD/urtc-smart-rack.elf" "$FIRMWARE_OUT/URTC_SMART_RACK_FIRMWARE_v${VERSION}.elf"
cp "$BUILD/urtc-smart-rack.bin" "$FIRMWARE_OUT/URTC_SMART_RACK_FIRMWARE_v${VERSION}.bin"
cp "$BUILD/urtc-smart-rack.hex" "$FIRMWARE_OUT/URTC_SMART_RACK_FIRMWARE_v${VERSION}.hex"
echo "  OK   firmware/URTC_SMART_RACK_FIRMWARE_v${VERSION}.{elf,bin,hex}"

echo ""
echo "============================================================================="
echo " Build complete - v${VERSION}"
echo "============================================================================="
