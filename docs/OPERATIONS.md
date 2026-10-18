<!-- =============================================================================
URTC-SMART-RACK - Operations guide
Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
GPL-3.0-or-later - see LICENSE
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
| Tool ID | 5 bits; `0`..`24` are the tool profiles of the URTC catalogue, `25`..`30` are unassigned, `31` is "free configuration" on a URTC head (and this firmware's marker for an empty slot) | `tool_id.h` |
| Idempotency table | one entry per possible tool ID (`1 << TOOL_ID_BITS` = 32) | `link_watchdog.h` |
| Pre-heat targets | soldering iron 200 °C, hot air 350 °C, generic 0 (none) | `preheat.c` |
| Hard upper bound on any commanded pre-heat | 400 °C | `rack_command.h` |
| Upper bound per tool | soldering iron and hot-air head 400 °C, 3D printer hotend 310 °C, every other profile has no heater (only 0 is accepted) | `tool_limits.c` |
| Link timeout when none (0) is configured | 250 ms | `link_watchdog.h` |

## Tool presence and inventory

- A raw 5-bit reading is masked to the tool ID. The ID space is the one of the
  URTC tool heads: `0`..`24` name a tool profile, `25`..`30` name nothing, and
  `31` means "free configuration" on a URTC head - its real tool is stored in the
  head's F-RAM, so the five bits alone do not identify it. The rack never heats a
  head by an ID of `25`..`31`.
- Polarity: the bits handed to `tool_id_decode()` are logical bits, with an
  installed jumper (line to ground) reading as 1, as a URTC head reads its own ID.
  A caller that reads raw pin levels must invert them first; a head with no jumper
  at all is tool 0 (a soldering iron), not an empty slot.
- Whether a bay holds a tool at all is decided by its presence sensor, not by an
  ID value (see reconciliation below). `31` appears inside the firmware only as the
  marker of an empty slot.
- Only a tool profile (`0`..`24`) can be racked; `rack_inventory_set_slot_tool()`
  refuses the rest.
- The inventory starts with every slot **empty**: a slot that has never been
  written is treated as unknown/empty, not assumed to hold a tool.
- A tool can be in **exactly one slot**. Racking it elsewhere clears its
  previous slot; racking `TOOL_ID_NONE` empties a slot.
- Out-of-range slot numbers are refused (reads return false and write
  nothing) instead of touching memory past the array.
- Pre-heat targets are stored per slot, and can only be set for a tool that
  is currently racked and only up to the limit of that tool (`tool_limits.c`;
  a tool without a heater accepts only 0). A pre-heat request for a tool that
  was already pulled fails rather than "succeeding" against a stale slot.
- Reconciliation (`rack_inventory_reconcile()`): at boot, and whenever the
  per-slot presence sensors change, the inventory is checked against them.
  A slot recorded as occupied whose sensor reads empty is cleared together
  with its pre-heat target (the tool was pulled while the rack was off); a
  slot whose sensor reads occupied while the inventory holds nothing is
  reported for identification and stays unoccupied, so it cannot be
  pre-heated until it is identified and racked.
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
is wrong, the tool byte is not a plain 5-bit value (any of the three upper bits
set is corruption, not "the same tool"), the tool is `25`..`30` or `31`, the
target is above 0 for a tool that has no heater, or the temperature exceeds the
limit of that tool (never more than 400 °C). A target of 0 (off) is accepted for
any tool.

This frame format is not a CAN frame. It can be up to 22 bytes long, a classic CAN
frame carries 8, and the temperature is little-endian while the URTC bus sends its
temperatures big-endian. No CAN identifiers are assigned to the rack. Until a
transport is designed, treat the format as the protocol of a serial link; mapping it
onto the bus means choosing identifiers, an 8-byte payload and a byte order, and
documenting them next to the CAN documentation of the URTC project.

Execution context: the watchdog state is plain memory with no locking. Call
`rack_link_decide()` and every `link_watchdog_*` function from one context, the main
loop. A receive interrupt should only copy the bytes into a ring buffer.

## Safe state and link loss

- If **no frame has ever arrived**, the link counts as lost - a link never
  proven alive is treated exactly like one that just died.
- `rack_app_tick()` runs in every pass of the main loop: it latches a loss
  (`link_watchdog_tick()`, so a rack that heard nothing for more than 49 days does
  not see the link come back to life when the millisecond counter wraps) and keeps
  every slot in the safe state while the link is lost. `rack_app_apply_decision()`
  turns the decision of `rack_link_decide()` into the target of the slot that holds
  the named tool.
- A link timeout of 0 cannot be honoured (the link would be lost at every instant
  and no resend could ever be recognised), so it is replaced by 250 ms.
- After the timeout with no valid frame, the safe pre-heat target is **off**
  (0 °C), whatever was last commanded. A lost link must never leave a heater
  running unattended.
- A frame that is not a command to act on (corrupt, a retransmission of a
  command already applied, a command this rack does not act on) **changes
  nothing**: noise on the bus or a resend must not switch a heater off. A
  command with an out-of-range value puts **that tool** in the safe state.
  Only a lost link brings every tool to the safe state. Every decision
  names the tool it is for (`rack_link_decide()`), so a rack with several
  tools never applies one tool's temperature to another.
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
| Command ignored | CRC mismatch, wrong version, a replayed sequence (the sender is acknowledged), or a tool byte that names nobody. Only a refused value for a known tool, or a lost link, switches a heater off |
| Pre-heat request refused | Tool not racked, tool byte with upper bits set or ID `25`..`31`, a target above 0 for a tool without a heater, or a target above the limit of that tool (400 °C at most) |
| Slot write returns false | Slot index outside 0-7, or a tool ID above 24 |
| Same tool reported in two places | Cannot happen: re-racking moves it and clears the old slot |
