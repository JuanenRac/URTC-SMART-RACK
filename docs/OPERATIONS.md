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
| Free-configuration query (heads whose jumpers read `0x1F`) | 250 ms per query, 3 queries per round, 2000 ms back-off, 5000 ms refresh (0 = never); until a valid answer the slot holds no tool | `free_tool.h` (`free_tool_config_t`) |
| Idempotency table | one entry per possible tool ID (`1 << TOOL_ID_BITS` = 32) | `link_watchdog.h` |
| Pre-heat targets | soldering iron 200 °C, hot air 350 °C, generic 0 (none) | `preheat.c` |
| Hard upper bound on any commanded pre-heat | 400 °C | `rack_command.h` |
| Upper bound per tool | soldering iron and hot-air head 400 °C, 3D printer hotend 310 °C, every other profile has no heater (only 0 is accepted) | `tool_limits.c` |
| Link timeout when none (0) is configured | 250 ms | `link_watchdog.h` |
| Longest pre-heat without a tool swap | 10 minutes (`RACK_PREHEAT_MAX_MS`, a build option; `preheat_limit_init()` takes the value at run time; `PREHEAT_LIMIT_OFF` disables the limit) | `preheat_limit.h` |
| Least time between two commands accepted for one tool | 20 ms (`RACK_COMMAND_MIN_INTERVAL_MS`, a build option; `link_watchdog_set_command_interval()` takes the value at run time, 0 = no limit). A command held back is not acknowledged and does not spend its sequence number; a safe state and a retransmission are never held back | `link_watchdog.h` |
| Silence that ends a half frame on the serial link | 20 ms (`RACK_DEFRAMER_DEFAULT_TIMEOUT_MS`; `rack_deframer_init()` takes the value at run time) | `rack_deframer.h` |

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
tools) have no pre-heat target. A 3D printer hotend has none by default either, because its temperature
depends on the filament: the plan pre-heats it only when `printer_target_c` is set in the configuration of the
plan, and never above the 310 degree ceiling of the tool. The decision and temperatures are real; the
heater control loop is not implemented - no PWM, no ADC, no closed loop.

### Longest pre-heat without a swap

A live link used to be able to keep a heater on for ever: the watchdog only catches a
dead one. `preheat_limit.c` bounds it. Per slot, the clock starts when the target goes
from 0 to something above 0; resending the target or raising it does not restart it.
When the limit (10 minutes unless configured) is reached, the target goes to 0 and the slot
is latched: a target above 0 is refused until the host sends a target of 0 or the tool in
the slot is replaced (a swap). A lost link, an emptied slot or another tool in the slot end
the pre-heat without a latch. The limit is part of the configuration table above and can be
turned off (`PREHEAT_LIMIT_OFF`), in which case only the link watchdog and the per-tool
ceilings bound the heater. `rack_app_apply_decision_limited()` and
`rack_app_tick_limited()` use it; the two functions without "limited" apply no limit.

## Lifecycle

Each tool has two counters, `total_cycles` and `total_seconds`. A tool is
flagged for maintenance once `total_cycles` reaches a threshold that the
caller supplies (different tool types wear at very different rates, so it is
not hardcoded). Persisting the counters to F-RAM is not implemented yet.

## Link protocol

Frame: `SOF(0xA5) | VERSION(2) | SEQ | CMD | LEN | PAYLOAD (max 16) | CRC8`.
CRC-8 (polynomial `0x07`) covers everything from SOF to the last payload
byte. A frame is accepted only if SOF, version, declared length and CRC are
all consistent; anything else is dropped whole and never partially trusted.
Version 2 is the big-endian layout: version 1 sent the temperature little-endian, and a
frame of that version is refused as an unsupported version instead of being read as
another temperature.

The only command today is `SET_PREHEAT` (`0x01`, payload: tool ID + target
temperature as 16-bit big-endian). It is rejected when the payload length
is wrong, the tool byte is not a plain 5-bit value (any of the three upper bits
set is corruption, not "the same tool"), the tool is `25`..`30` or `31`, the
target is above 0 for a tool that has no heater, or the temperature exceeds the
limit of that tool (never more than 400 °C). A target of 0 (off) is accepted for
any tool.

### Answers to the host

Every frame that is well formed (framing and CRC right) gets a reason, in one byte, in the decision of `rack_link_decide()` (`reply`, with the command, the tool byte and the sequence number it is about), and `rack_reply_encode()` turns it into a frame of the same framing: command `0x81` (acknowledgement) or `0x82` (refusal), the sequence number of the frame it answers, and the payload `command answered`, `tool byte as sent` (`0xFF` when the command carries none) and the code.

| Code | Name | Meaning |
|---:|---|---|
| 0 | `APPLIED` | the command was applied (acknowledgement) |
| 1 | `REPEAT` | already applied: a retransmission or a stale sequence; the sender can stop resending (acknowledgement) |
| 2 | `WRONG_LENGTH` | the payload is not the size the command has |
| 3 | `TOOL_ABSENT` | the tool byte says the slot holds nothing |
| 4 | `TEMP_OUT_OF_RANGE` | the target is above what the rack, or that tool, accepts |
| 5 | `TOOL_INVALID` | not a plain 5-bit value, or no tool profile has that number |
| 6 | `NOT_HEATABLE` | a target above 0 for a tool without a heater |
| 7 | `HELD_RATE` | too soon after the previous command accepted for that tool; send it again later |
| 8 | `UNKNOWN_COMMAND` | a well-formed frame with a command this rack does not have |

A frame that does not pass the framing and the CRC, or a CAN frame that is not for this rack, is not answered: noise must not provoke traffic. The reply is additive: a host that does not know the commands `0x81` and `0x82` ignores them, and nothing that was accepted or refused before changes. No transport sends the frame on a board yet. The host model reads it: the simulated rack benches of URTC-TESTER and URTC-WEB-STUDIO write and read the same frame (a refusal is shown with its reason in the language of the program), and the host tests hold `rack_reply_encode()` to the reply of every step of the shared scenarios of URTC-COMMON, byte for byte.

### Transport

The transport that is implemented is a **serial link** with the frame above. It can be up
to 22 bytes long, so it is not a CAN frame, and its temperature is big-endian, like the CAN form below. A serial
link delivers bytes, not frames, so `rack_deframer.c` finds them in the stream:

- it waits for the start byte and collects the bytes the header announces (the header is
  checked as soon as it is complete: an unsupported version or a length above 16 is refused
  without waiting for the rest);
- it hands over a frame only when the CRC is right;
- after a bad candidate it drops the first byte and starts again from the next start byte it
  already holds, so a byte lost inside a frame costs that frame and not the next one, and a
  start byte inside a payload does not make it give up;
- a half frame is dropped after 20 ms of silence (configurable), so a start byte that was
  noise cannot swallow the next real frame; the comparison is correct across the wrap of the
  millisecond counter;
- it counts good frames, CRC errors, bad headers, resynchronisations, timeouts and dropped
  bytes, so a bench can tell a noisy line from a quiet one.

`rack_link_rx_byte()` takes one received byte and, when it completes a frame, gives the
decision about the heater; `rack_link_rx_next()` is called afterwards until it returns false,
because after a resynchronisation the deframer can hold a second frame.

### CAN route (when a board exists)

No board with a CAN transceiver exists, so nothing below has been on a bus. The mapping is
decided now (`rack_can.c`, tested on the host) so the byte order and the size are not chosen
after the hardware. A classic CAN frame carries 8 bytes and has its own hardware CRC, so the
start byte, version, length and CRC-8 of the serial frame are not repeated, and every
multi-byte value is **big-endian**, like every temperature on the URTC bus.

| Frame | Direction | Identifier | DLC | Data |
| --- | --- | --- | --- | --- |
| `SET_PREHEAT` | host to rack | `0x260` | 8 | byte 0: tool ID; bytes 1-2: target in whole degrees C, big-endian; byte 3: sequence number; bytes 4-7: reserved, sent as 0, ignored on receipt |
| `REPLY` | rack to host | `0x261` | 3 | byte 0: the command answered (`0x01`, `SET_PREHEAT`); byte 1: the tool byte the command carried (`0xFF`: none); byte 2: the reason (`0` applied, `1` repeat: acknowledgements; `2` wrong length, `3` tool absent, `4` temperature out of range, `5` tool invalid, `6` not heatable, `7` held (too soon), `8` unknown command: refusals) |

The reply is the same three bytes as the payload of the serial acknowledgement (`0x81`) and
refusal (`0x82`); on CAN the kind is told by the reason, and there is no sequence number, so the
host matches a reply with its order by the command and the tool. `rack_can_encode_reply()` writes it
from the decision of the link and `rack_can_parse_reply()` reads it; nothing sends it yet because no
board with a CAN transceiver exists. That code and its host tests (`tests/test_rack_can.c` and the shared
vectors) were written without a compiler at hand: they have not been built or run, and nothing has been on a
bus.

The identifiers `0x260`..`0x26F` are the block the specification of URTC-COMMON keeps for the
rack. A command that arrives this way goes through the same rules as a serial one
(`rack_link_decide_can()`): the same range checks, the same per-tool anti-replay sequence, the
same safe state for a refused value. A frame with another identifier or another length is not
ours and changes nothing, not even the link watchdog.

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
| Command ignored | Bytes lost on the line (the deframer counts CRC errors, bad headers and timeouts), CRC mismatch, wrong version, a replayed sequence (the sender is acknowledged), or a tool byte that names nobody. Only a refused value for a known tool, or a lost link, switches a heater off |
| Pre-heat request refused | Tool not racked, tool byte with upper bits set or ID `25`..`31`, a target above 0 for a tool without a heater, or a target above the limit of that tool (400 °C at most) |
| Pre-heat switched off while the link is alive | The slot ran out its pre-heat (10 minutes unless configured); send a target of 0, or swap the tool, before heating it again |
| Slot write returns false | Slot index outside 0-7, or a tool ID above 24 |
| Same tool reported in two places | Cannot happen: re-racking moves it and clears the old slot |
