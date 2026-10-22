@echo off
REM HYDRA_UMC_SCRIPT_STANDARD_HEADER_BEGIN
REM *****************************************************************************
REM Project   : URTC-SMART-RACK
REM Script    : build_firmware.bat
REM Purpose   : Incremental firmware build and versioned artifact packaging workflow.
REM Author    : JuanenRac (Electro Hobby 3D)
REM Email     : electrohobby3d@gmail.com
REM Copyright : (C) 2026 JuanenRac
REM License   : GPL-3.0-or-later - see LICENSE
REM *****************************************************************************
REM HYDRA_UMC_SCRIPT_STANDARD_HEADER_END
REM HYDRA_UMC_SCRIPT_STANDARD_BANNER_BEGIN
echo.
echo *****************************************************************************
echo * URTC-SMART-RACK - build_firmware.bat
echo * Mode      : BUILD (no version change)
echo * Author    : JuanenRac (Electro Hobby 3D)
echo * Email     : electrohobby3d@gmail.com
echo * Copyright : (C) 2026 JuanenRac
echo * License   : GPL-3.0-or-later - see LICENSE
echo * ------------------------------------------------------------------------- *
echo * 1. Read the project version (compiling never changes it; run bump.py to raise it).
echo * 2. Run this project's declared build, verification and packaging commands.
echo * 3. Report the result and keep an interactive terminal open.
echo *****************************************************************************
echo.
REM HYDRA_UMC_SCRIPT_STANDARD_BANNER_END
REM URTC-SMART-RACK - Firmware Build Script
REM Compiles this project's minimal Cortex-M4F skeleton (src/main.c +
REM src/startup_stm32g4_minimal.c - see those files' own header comments for
REM why there's no ST HAL/CMSIS dependency yet, no real PCB exists to pin
REM down the exact STM32G4 part). Same arm-none-eabi-gcc toolchain as
REM sibling repo URTC/build_firmware.bat, reused rather than reinvented, but
REM with the HAL/CMSIS fetch steps skipped entirely - nothing chip-specific
REM to fetch yet at this project's current andamiaje (scaffolding) stage.
setlocal enabledelayedexpansion
rem Keep the window open for people, never in automation (CI / URTC_CI).
set "KEEP_OPEN=pause"
if defined CI set "KEEP_OPEN=ver >nul"
if "%URTC_CI%"=="1" set "KEEP_OPEN=ver >nul"
rem Every path below is relative to the repository root, whatever the directory the script is started from.
cd /d "%~dp0"
set "VERSION_BUMPED=0"
if "%URTC_CI%"=="1" goto :banner_ci
REM HYDRA_UMC_SCRIPT_STANDARD_VERSION_STEP
echo [1/3] Reading the project version (compiling never changes it; raise it with bump.py first)...
REM HYDRA_UMC_SCRIPT_STANDARD_VERSION_CAPTURE_BEFORE
for /f "usebackq delims=" %%V in (`python -c "import json; print(json.load(open(r'%~dp0urtc.project.json', encoding='utf-8'))['version'])"`) do set "HYDRA_UMC_VERSION_BEFORE=%%V"
goto :banner_done
:banner_ci
echo URTC-SMART-RACK CI: version sources are read-only.
:banner_done
echo.
where arm-none-eabi-gcc >nul 2>nul
if errorlevel 1 (
    echo FAIL: arm-none-eabi-gcc not found. Install the ARM GNU Toolchain
    echo       (e.g. "choco install gcc-arm-embedded"^), then re-run.
    goto :error
)
echo   OK   arm-none-eabi-gcc found
echo.

echo === 2. Real logic tests (host cl/gcc, not arm-none-eabi-gcc) ===
if not exist build mkdir build
where gcc >nul 2>nul
if errorlevel 1 (
    echo FAIL: no host C compiler found ^(gcc^). Install MinGW/MSYS2 or run
    echo       this from a shell that has a native gcc on PATH.
    goto :error
)
gcc -std=c11 -Wall -Wextra -Wconversion -Isrc -Itests -o build\host_tests.exe tests\test_main.c tests\test_tool_id.c tests\test_lifecycle.c tests\test_preheat.c tests\test_protocol.c tests\test_rack_command.c tests\test_link_watchdog.c tests\test_rack_inventory.c tests\test_robustness.c tests\test_rack_app.c tests\test_rack_link_scenarios.c tests\test_rack_deframer.c tests\test_rack_can.c tests\test_preheat_limit.c src\tool_id.c src\tool_limits.c src\lifecycle.c src\preheat.c src\protocol.c src\rack_command.c src\link_watchdog.c src\rack_inventory.c src\rack_app.c src\rack_link.c src\rack_deframer.c src\rack_can.c src\preheat_limit.c
if errorlevel 1 goto :error
build\host_tests.exe
if errorlevel 1 goto :error
echo   OK   tool_id.c / tool_limits.c / lifecycle.c / preheat.c / protocol.c / rack_command.c / link_watchdog.c / rack_link.c / rack_inventory.c / rack_deframer.c / rack_can.c / preheat_limit.c pure-logic tests passed
echo.

echo === 2b. Build tooling tests ^(version odometer boundaries^) ===
python -m unittest discover -s tests -p "test_*.py"
if errorlevel 1 goto :error
echo.

echo === 3. Firmware version (read from the header; bump.py raises it, building never does) ===
rem Compiling never changes the version: it is read from the header (raise it first with bump.py).
for /f "tokens=3" %%A in ('findstr /b /c:"#define FIRMWARE_VERSION_MAJOR" src\firmware_common.h') do set "V_MAJOR=%%A"
for /f "tokens=3" %%A in ('findstr /b /c:"#define FIRMWARE_VERSION_MINOR" src\firmware_common.h') do set "V_MINOR=%%A"
for /f "tokens=3" %%A in ('findstr /b /c:"#define FIRMWARE_VERSION_PATCH" src\firmware_common.h') do set "V_PATCH=%%A"
set "VERSION=%V_MAJOR%.%V_MINOR%.%V_PATCH%"

echo   Firmware version: %VERSION%
echo.

echo === 4. Compile + link ===
if not exist build mkdir build
REM Release mode writes to firmware\ (git-ignored, published only as release assets); CI mode writes to build\firmware.
set "FIRMWARE_OUT=firmware"
if "%URTC_CI%"=="1" set "FIRMWARE_OUT=build\firmware"
if not exist %FIRMWARE_OUT% mkdir %FIRMWARE_OUT%
if not exist build\arm mkdir build\arm

set CFLAGS=-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -Wall -Wextra -Wconversion -O2 -g -Isrc
set LDFLAGS=-T src\STM32G4_MINIMAL.ld -nostdlib -Wl,--gc-sections -Wl,-Map=build\urtc-smart-rack.map

rem Every module is compiled for the Cortex-M4F target, not only the two that the image links today.
for %%S in (src\*.c) do (
    arm-none-eabi-gcc %CFLAGS% -c %%S -o build\arm\%%~nS.o
    if errorlevel 1 goto :error
)
echo   OK   every src\*.c compiled for Cortex-M4F

arm-none-eabi-gcc %CFLAGS% %LDFLAGS% build\arm\startup_stm32g4_minimal.o build\arm\main.o -o build\urtc-smart-rack.elf
if errorlevel 1 goto :error
echo   OK   Linked build\urtc-smart-rack.elf

rem All modules must also resolve against each other without a C library; this link is only a check and produces no image.
arm-none-eabi-gcc %CFLAGS% -T src\STM32G4_MINIMAL.ld -nostdlib -Wl,--gc-sections build\arm\*.o -o build\all-modules-check.elf
if errorlevel 1 goto :error
echo   OK   all modules link together without a C library

arm-none-eabi-objcopy -O binary build\urtc-smart-rack.elf build\urtc-smart-rack.bin
if errorlevel 1 goto :error
arm-none-eabi-objcopy -O ihex build\urtc-smart-rack.elf build\urtc-smart-rack.hex
if errorlevel 1 goto :error
echo   OK   build\urtc-smart-rack.bin / .hex
echo.

echo === 5. Size report ===
arm-none-eabi-size build\urtc-smart-rack.elf
echo.

echo === 6. Publish versioned artifacts to firmware\ ===
copy /y build\urtc-smart-rack.elf %FIRMWARE_OUT%\URTC_SMART_RACK_FIRMWARE_v%VERSION%.elf >nul
copy /y build\urtc-smart-rack.bin %FIRMWARE_OUT%\URTC_SMART_RACK_FIRMWARE_v%VERSION%.bin >nul
copy /y build\urtc-smart-rack.hex %FIRMWARE_OUT%\URTC_SMART_RACK_FIRMWARE_v%VERSION%.hex >nul
echo   OK   firmware\URTC_SMART_RACK_FIRMWARE_v%VERSION%.{elf,bin,hex}
echo.

echo =============================================================================
echo  Build complete - v%VERSION%
echo =============================================================================
%KEEP_OPEN%
exit /b 0

:error
echo.
if "%VERSION_BUMPED%"=="1" (
    copy /y build\version_backup\src\firmware_common.h src\firmware_common.h >nul
    copy /y build\version_backup\urtc.project.json urtc.project.json >nul
    copy /y build\version_backup\CHANGELOG.md CHANGELOG.md >nul
    echo Version, manifest and changelog restored to their previous state.
)
echo BUILD FAILED - see the output above.
%KEEP_OPEN%
exit /b 1
