@echo off
REM =============================================================================
REM URTC-SMART-RACK - Firmware Build Script
REM Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
REM GPL-3.0 - see LICENSE
REM =============================================================================
REM Compiles this project's minimal Cortex-M4F skeleton (src/main.c +
REM src/startup_stm32g4_minimal.c - see those files' own header comments for
REM why there's no ST HAL/CMSIS dependency yet, no real PCB exists to pin
REM down the exact STM32G4 part). Same arm-none-eabi-gcc toolchain as
REM sibling repo URTC/build_firmware.bat, reused rather than reinvented, but
REM with the HAL/CMSIS fetch steps skipped entirely - nothing chip-specific
REM to fetch yet at this project's current andamiaje (scaffolding) stage.
setlocal enabledelayedexpansion
python "%~dp0bump_manifest_version.py"
if errorlevel 1 ( echo VERSION BUMP FAILED. & pause & exit /b 1 )
cd /d "%~dp0"

echo =============================================================================
echo  URTC-SMART-RACK - firmware build
echo.
echo  Compiles the minimal Cortex-M4F skeleton (no ST HAL yet - see
echo  src/firmware_common.h for why) with arm-none-eabi-gcc.
echo.
echo  Author:  JuanenRac (Electro Hobby 3D) - electrohobby3d@gmail.com
echo  License: GPL-3.0 - see LICENSE
echo =============================================================================
echo.

echo === 1. Toolchain ===
where arm-none-eabi-gcc >nul 2>nul
if errorlevel 1 (
    echo FAIL: arm-none-eabi-gcc not found. Install the ARM GNU Toolchain
    echo       (e.g. "choco install gcc-arm-embedded"^), then re-run.
    goto :error
)
echo   OK   arm-none-eabi-gcc found
echo.

echo === 2. Version bump (odometer, see bump_version.py) ===
for /f "delims=" %%V in ('python bump_version.py src\firmware_common.h FIRMWARE_VERSION') do set VERSION=%%V
if "%VERSION%"=="" goto :error
echo   Firmware version: %VERSION%
echo.

echo === 3. Compile + link ===
if not exist build mkdir build
if not exist firmware mkdir firmware

set CFLAGS=-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -ffreestanding -fno-builtin -Wall -Wextra -O2 -g -Isrc
set LDFLAGS=-T src\STM32G4_MINIMAL.ld -nostdlib -Wl,--gc-sections -Wl,-Map=build\urtc-smart-rack.map

arm-none-eabi-gcc %CFLAGS% -c src\startup_stm32g4_minimal.c -o build\startup_stm32g4_minimal.o
if errorlevel 1 goto :error
arm-none-eabi-gcc %CFLAGS% -c src\main.c -o build\main.o
if errorlevel 1 goto :error
arm-none-eabi-gcc %CFLAGS% %LDFLAGS% build\startup_stm32g4_minimal.o build\main.o -o build\urtc-smart-rack.elf
if errorlevel 1 goto :error
echo   OK   Linked build\urtc-smart-rack.elf

arm-none-eabi-objcopy -O binary build\urtc-smart-rack.elf build\urtc-smart-rack.bin
arm-none-eabi-objcopy -O ihex build\urtc-smart-rack.elf build\urtc-smart-rack.hex
echo   OK   build\urtc-smart-rack.bin / .hex
echo.

echo === 4. Size report ===
arm-none-eabi-size build\urtc-smart-rack.elf
echo.

echo === 5. Publish versioned artifacts to firmware\ ===
copy /y build\urtc-smart-rack.elf firmware\URTC_SMART_RACK_FIRMWARE_v%VERSION%.elf >nul
copy /y build\urtc-smart-rack.bin firmware\URTC_SMART_RACK_FIRMWARE_v%VERSION%.bin >nul
copy /y build\urtc-smart-rack.hex firmware\URTC_SMART_RACK_FIRMWARE_v%VERSION%.hex >nul
echo   OK   firmware\URTC_SMART_RACK_FIRMWARE_v%VERSION%.{elf,bin,hex}
echo.

echo =============================================================================
echo  Build complete - v%VERSION%
echo =============================================================================
exit /b 0

:error
echo.
echo BUILD FAILED - see the output above.
exit /b 1
