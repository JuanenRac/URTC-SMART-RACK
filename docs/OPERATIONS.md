<!-- =============================================================================
URTC-SMART-RACK - Operations guide
Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
GPL-3.0 - see LICENSE
============================================================================= -->

# Operations Guide

How the rack's logic behaves, what it will refuse, and what is simulated.
There is **no PCB for this board yet**, so nothing here drives real GPIO,
F-RAM, CAN or a heater: every module is pure C, host-testable logic that will
be wired to hardware later. Read every "physical" statement below as "what
the logic decides", not "what a real rack does".

## Configuration and capacity

| Item | Value | Where |
| --- | --- | --- |
| Tool slots in the inventory | 8 (`RACK_INVENTORY_MAX_SLOTS`) | `rack_inventory.h` - a placeholder capacity, not a hardware claim |
| Tool ID | 5 bits, `0x1F` = no tool present | `tool_id.h` |
| Idempotency table | one entry per possible tool ID (32) | `link_watchdog.h` |
| Pre-heat targets | soldering iron 200 °C, hot air 350 °C, generic 0 (none) | `preheat.c` |
| Hard upper bound on any commanded pre-heat | 400 °C | `rack_command.h` |

## Tool presence and inventory

- A raw 5-bit reading is masked to the tool ID; all-ones means "nothing
  plugged in" (floating lines on their pull-ups), never a valid slot.
- The inventory starts with every slot **empty**: a slot that has never been
  written is treated as unknown/empty, not assumed to hold a tool.
- A tool can be in **exactly one slot**. Racking it elsewhere clears its
  previous slot; racking `TOOL_ID_NONE` empties a slot.
- Out-of-range slot numbers are refused (reads return false and write
  nothing) instead of touching memory past the array.
- Pre-heat targets are stored per slot, and can only be set for a tool that
  is currently racked. A pre-heat request for a tool that was already
  pulled fails rather than "succeeding" against a stale slot.
- Simulated vs physical: today the ID comes from whatever the caller passes
  in. Reading real jumpers or F-RAM needs the PCB.

## Pre-heat ("Smart Idle")

Pre-heating starts only when an anticipated swap is within the configured
lead time and has not already begun. Generic tools (grippers, no-contact
tools) have no pre-heat target. The decision and temperatures are real; the
heater control loop is not implemented - no PWM, no ADC, no closed loop.

## Lifecycle

Each tool has two counters, `total_cycles` and `total_seconds`. A tool is
flagged for maintenance once `total_cycles` reaches a threshold that the
caller supplies (different tool types wear at very different rates, so it is
not hardcoded). Persisting the counters to F-RAM is not implemented yet.

## Link protocol

Frame: `SOF(0xA5) | VERSION(1) | SEQ | CMD | LEN | PAYLOAD (max 16) | CRC8`.
CRC-8 (polynomial `0x07`) covers everything from SOF to the last payload
byte. A frame is accepted only if SOF, version, declared length and CRC are
all consistent; anything else is dropped whole and never partially trusted.

The only command today is `SET_PREHEAT` (`0x01`, payload: tool ID + target
temperature as 16-bit little-endian). It is rejected when the payload length
is wrong, the tool slot is absent, or the temperature exceeds 400 °C.

## Safe state and link loss

- If **no frame has ever arrived**, the link counts as lost - a link never
  proven alive is treated exactly like one that just died.
- After the timeout with no valid frame, the safe pre-heat target is **off**
  (0 °C), whatever was last commanded. A lost link must never leave a heater
  running unattended.
- Any frame that fails at any layer (corrupt, duplicate, unrelated command,
  out-of-range value) also resolves to the safe target, never to "keep the
  previous value".
- Replayed or reordered commands are rejected per tool with a sequence check
  that handles 8-bit wraparound. When the link had been lost, the sequence
  baseline is reset so a new session is not compared against the old one.
  This is not cryptographic protection.

## What is not verified

No test has run against a real board, transceiver, heater or sensor. Treat
every physical behaviour as unproven until a bench test exists.

## Troubleshooting

| Symptom | Likely cause |
| --- | --- |
| Command always ends in "off" | CRC mismatch, wrong version, a replayed sequence, or the link timed out |
| Pre-heat request refused | Tool not racked, ID is `0x1F` (none), or target above 400 °C |
| Slot write returns false | Slot index outside 0-7 |
| Same tool reported in two places | Cannot happen: re-racking moves it and clears the old slot |
