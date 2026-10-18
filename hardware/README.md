<!-- =============================================================================
URTC-SMART-RACK - Hardware notes
Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
GPL-3.0-or-later - see LICENSE
============================================================================= -->

# Hardware

There is **no hardware design for this board yet**: no schematic, no PCB layout, no
Gerber files and no confirmed microcontroller part. This folder is the place where
those files will go once they exist.

What is decided so far, all of it provisional:

- the target family is an STM32G4 (Cortex-M4F); the exact part and therefore its flash
  and RAM sizes are open (the linker script assumes the smallest members of the family);
- the rack is meant to hold several tool heads, identify each one, and pre-heat the ones
  that have a heater; the number of bays is a placeholder (`RACK_INVENTORY_MAX_SLOTS`);
- identification would come from the 5-bit tool ID of the head, plus one presence sensor
  per bay (see `docs/OPERATIONS.md`).

Nothing in `src/` has ever driven a real pin, sensor or heater.
