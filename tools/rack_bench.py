#!/usr/bin/env python3
# =============================================================================
# URTC-SMART-RACK - tools/rack_bench.py
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
# GPL-3.0-or-later - see LICENSE
# =============================================================================
"""The software bench of the rack: scenarios with faults played through the real rack logic, with no board.

The rack logic of the firmware (receive ring, deframer, link decision, watchdog, inventory, pre-heat limit, replies) is compiled for the host and put behind a
text port (tests/virtual_rack.c). This tool builds it, plays scenarios through it - good commands, noise, damaged frames, repeats, a silent link, a pre-heat
that runs out, and seeded random runs with injected faults - and checks every answer two ways:

* against what each scenario must produce (written by hand from the rules of the rack, never taken from a run);
* against the Python model of the rack (urtc_common.rack_sim, when it can be imported): the same events go through both and the replies and the state of the
  eight slots must be identical after every step. A difference means that the firmware and the model disagree about the protocol.

    python tools/rack_bench.py                   # build and run every scenario (CI sets the lock aside; elsewhere ALLOW_LOCAL_BUILD=1)
    python tools/rack_bench.py --list            # the scenarios
    python tools/rack_bench.py --only NAME       # one scenario (repeatable)
    python tools/rack_bench.py --seed 7 --events 3000
    python tools/rack_bench.py --dump DIR        # write the script of every scenario (replay with --replay)
    python tools/rack_bench.py --replay FILE     # play a recorded script through the virtual rack and print what it answered
    python tools/rack_bench.py --no-model        # do not compare with the Python model
    python tools/rack_bench.py --common DIR      # where urtc_common lives (a checkout of URTC-COMMON/src); the variable URTC_COMMON_SRC does the same

This is a rack with no hardware: the run says nothing about the board, the driver of the heater or the real bus. Exit status: 0 passed, 1 a check failed, 2 skipped
because no host compiler was found or local builds are locked.
"""
from __future__ import annotations

import argparse
import os
import random
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Mapping

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
try:
    from local_build import local_build_refusal  # noqa: E402  (the lock on builds that compile on this PC: copied by vendor_copy.py)
except ImportError:   # a repository that has not taken the helper yet keeps the same rule
    def local_build_refusal(env: Mapping[str, str] | None = None) -> str | None:
        env = os.environ if env is None else env
        if env.get("ALLOW_LOCAL_BUILD", "").strip() == "1" or any(env.get(n, "").strip().lower() not in ("", "0", "false", "no", "off") for n in ("CI", "GITHUB_ACTIONS")):
            return None
        return "local C builds are off; GitHub Actions builds it. Set ALLOW_LOCAL_BUILD=1 to allow it on this PC"

SOURCES = ["tests/virtual_rack.c", "src/link_watchdog.c", "src/preheat.c", "src/preheat_limit.c", "src/protocol.c", "src/rack_app.c", "src/rack_can.c", "src/rack_command.c",
           "src/rack_deframer.c", "src/rack_inventory.c", "src/rack_link.c", "src/rack_reply.c", "src/rack_ring.c", "src/rack_rx.c", "src/tool_id.c", "src/tool_limits.c"]
INCLUDES = ["src", "tests"]
FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Wconversion", "-O1", "-g"]
SANITIZE = ["-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]

# The protocol of the rack (the same numbers as src/protocol.h, src/rack_link.h and src/link_watchdog.h)
SOF, VERSION, CMD_SET_PREHEAT = 0xA5, 2, 0x01
CAN_ID_SET_PREHEAT, CAN_ID_REPLY = 0x260, 0x261
REPLY_CODES = ("applied", "repeat", "wrong_length", "tool_absent", "temp_out_of_range", "tool_invalid", "not_heatable", "held_rate", "unknown_command")
SLOTS = 8
TOOL_IRON, TOOL_PRINTER, TOOL_LASER_ISH, TOOL_NO_HEATER = 0, 19, 10, 3   # 0 soldering iron (400 C), 19 3D printer (400 C), 10 a tool limited to 310 C, 3 a tool with no heater
PREHEAT_MAX_MS = 600_000


def crc8(data: bytes) -> int:
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def frame(seq: int, cmd: int, payload: bytes = b"") -> bytes:
    body = bytes([SOF, VERSION, seq & 0xFF, cmd & 0xFF, len(payload)]) + payload
    return body + bytes([crc8(body)])


def set_preheat(seq: int, tool: int, temp: int) -> bytes:
    return frame(seq, CMD_SET_PREHEAT, bytes([tool, (temp >> 8) & 0xFF, temp & 0xFF]))


def set_preheat_can(seq: int, tool: int, temp: int) -> bytes:
    return bytes([tool, (temp >> 8) & 0xFF, temp & 0xFF, seq & 0xFF, 0, 0, 0, 0])


# ---- events ---------------------------------------------------------------------------------------------------------------------------------------
@dataclass(frozen=True)
class Event:
    at: int
    kind: str                      # "slot" | "serial" | "burst" | "can" | "tick"
    data: bytes = b""
    args: tuple[int, ...] = ()     # slot: (slot, tool, serial); can: (identifier,)


def slot(at: int, number: int, tool: int, serial: int = 0) -> Event:
    return Event(at, "slot", args=(number, tool, serial))


def serial(at: int, data: bytes) -> Event:
    return Event(at, "serial", data)


def burst(at: int, data: bytes) -> Event:
    return Event(at, "burst", data)


def can(at: int, data: bytes, identifier: int = CAN_ID_SET_PREHEAT) -> Event:
    return Event(at, "can", data, (identifier,))


def tick(at: int) -> Event:
    return Event(at, "tick")


def script_of(events: list[Event]) -> str:
    """The text the virtual rack reads: the clock, the event, a marker, and a request for the state after each."""
    lines: list[str] = []
    for index, event in enumerate(events):
        lines.append(f"T {event.at}")
        if event.kind == "slot":
            lines.append("I {} {} {}".format(*event.args))
        elif event.kind == "serial":
            lines.append("S " + event.data.hex())
        elif event.kind == "burst":
            lines.append("B " + event.data.hex())
        elif event.kind == "can":
            lines.append(f"C {event.args[0]:x} " + event.data.hex())
        lines.append("Q")
        lines.append(f"M {index}")
    return "\n".join(lines) + "\n"


# ---- what the rack answered ----------------------------------------------------------------------------------------------------------------------
@dataclass
class Step:
    replies: list[tuple[str, bytes]] = field(default_factory=list)       # ("S", frame) or ("C", three data bytes)
    slots: list[tuple[int, int, int, int, int]] = field(default_factory=list)   # occupied, tool, serial, target, expired
    link_lost: bool = True
    dropped: int = 0


def reason_of(reply: tuple[str, bytes]) -> str:
    kind, data = reply
    code = data[2] if kind == "C" else data[7] if len(data) >= 9 else -1
    return REPLY_CODES[code] if 0 <= code < len(REPLY_CODES) else f"code{code}"


def parse_output(text: str, count: int) -> list[Step]:
    steps: list[Step] = []
    current = Step()
    for line in text.splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "S":
            current.replies.append(("S", bytes.fromhex(parts[1])))
        elif parts[0] == "C":
            current.replies.append(("C", bytes.fromhex(parts[2])))
        elif parts[0] == "Q":
            if parts[1] == "link":
                current.link_lost = parts[2] == "1"
            elif parts[1] == "dropped":
                current.dropped = int(parts[2])
            else:
                current.slots.append((int(parts[2]), int(parts[3]), int(parts[4]), int(parts[5]), int(parts[6])))
        elif parts[0] == "M":
            steps.append(current)
            current = Step()
        else:
            raise RuntimeError(f"the virtual rack printed a line that is not part of its protocol: {line!r}")
    if len(steps) != count:
        raise RuntimeError(f"the virtual rack answered {len(steps)} of {count} steps")
    return steps


# ---- the build ------------------------------------------------------------------------------------------------------------------------------------
def _wsl_path(value: str) -> str:
    drive, rest = os.path.splitdrive(value)
    return f"/mnt/{drive[:1].lower()}{rest.replace(os.sep, '/')}" if drive else value.replace(os.sep, "/")


def build(binary: Path) -> list[str] | None:
    """Compiles the virtual rack and returns the command that runs it (through WSL on Windows), or None when it cannot be built here."""
    if local_build_refusal() is not None:
        return None
    flags = [*FLAGS, *(SANITIZE if os.environ.get("URTC_SANITIZE", "") == "1" else [])]
    inc = [f"-I{i}" for i in INCLUDES]
    if os.name == "nt":
        wsl = shutil.which("wsl")
        if wsl is None:
            return None
        out = _wsl_path(str(binary))
        if subprocess.run([wsl, "--cd", str(ROOT), "--", "gcc", *flags, *inc, "-o", out, *SOURCES]).returncode != 0:
            raise SystemExit("RACK_BENCH=FAIL the virtual rack did not compile")
        return [wsl, "--cd", str(ROOT), "--", out]
    cc = shutil.which("cc") or shutil.which("gcc")
    if cc is None:
        return None
    if subprocess.run([cc, *flags, *inc, "-o", str(binary), *SOURCES], cwd=ROOT).returncode != 0:
        raise SystemExit("RACK_BENCH=FAIL the virtual rack did not compile")
    return [str(binary)]


def play(prefix: list[str], events: list[Event], timeout: float = 120.0) -> list[Step]:
    done = subprocess.run(prefix, input=script_of(events), capture_output=True, text=True, timeout=timeout, check=False)
    if done.returncode != 0:
        raise RuntimeError(f"the virtual rack stopped with status {done.returncode}: {done.stderr.strip()[:2000]}")
    return parse_output(done.stdout, len(events))


# ---- the Python model of the rack ----------------------------------------------------------------------------------------------------------------
def import_model(common: str | None) -> Any:
    for candidate in (common, os.environ.get("URTC_COMMON_SRC")):
        if candidate and candidate not in sys.path:
            sys.path.insert(0, candidate)
    try:
        from urtc_common import rack_sim  # type: ignore[import-not-found]
    except ImportError:
        return None
    return rack_sim


def model_steps(rack_sim: Any, events: list[Event]) -> list[Step]:
    rack = rack_sim.SimulatedRack()
    steps: list[Step] = []
    for event in events:
        step = Step()
        rack.tick(event.at)
        if event.kind == "slot":
            rack.set_slot(event.args[0], event.args[1], event.args[2])
        elif event.kind in ("serial", "burst"):
            for reply in rack.receive_stream(event.data, event.at):
                if reply.frame:
                    step.replies.append(("S", bytes(reply.frame)))
        elif event.kind == "can":
            reply = rack.receive_can(event.args[0], event.data, event.at)
            if reply.can_frame is not None:
                step.replies.append(("C", bytes(reply.can_frame[1])))
        for index, entry in enumerate(rack.slots):
            step.slots.append((1 if entry.occupied else 0, entry.tool_id if entry.occupied else 31, entry.serial if entry.occupied else 0, entry.target_c,
                               1 if index in rack.expired else 0))
        step.link_lost = bool(rack.link_lost(event.at))
        steps.append(step)
    return steps


def compare(name: str, firmware: list[Step], model: list[Step], events: list[Event]) -> list[str]:
    problems: list[str] = []
    for index, (a, b) in enumerate(zip(firmware, model)):
        if events[index].kind == "burst" and len(events[index].data) > 40:
            continue        # a burst longer than the receive ring loses bytes by design; the model has no ring
        where = f"{name} step {index} at {events[index].at} ms ({events[index].kind})"
        if [(k, d.hex()) for k, d in a.replies] != [(k, d.hex()) for k, d in b.replies]:
            problems.append(f"{where}: replies differ: firmware {[(k, d.hex()) for k, d in a.replies]} model {[(k, d.hex()) for k, d in b.replies]}")
        if a.slots != b.slots:
            problems.append(f"{where}: slots differ: firmware {a.slots} model {b.slots}")
        if a.link_lost != b.link_lost:
            problems.append(f"{where}: link lost differs: firmware {a.link_lost} model {b.link_lost}")
        if len(problems) >= 5:
            break
    return problems


# ---- scenarios ------------------------------------------------------------------------------------------------------------------------------------
@dataclass
class Scenario:
    name: str
    about: str
    events: list[Event]
    replies: list[str] | None = None                 # the reason of each reply in order (None: not checked here, only against the model)
    targets: dict[int, int] | None = None            # the target of these slots at the end
    link_lost: bool | None = None                    # whether the link is lost at the end
    invariant: Callable[[list[Step]], list[str]] | None = None


RACK = [slot(0, 0, TOOL_IRON, 1), slot(0, 1, TOOL_PRINTER, 2), slot(0, 2, TOOL_LASER_ISH, 3)]


def fixed_scenarios() -> list[Scenario]:
    out: list[Scenario] = []
    out.append(Scenario("happy_path", "a serial and a CAN command set two targets and the link stays alive", [*RACK,
        serial(100, set_preheat(1, TOOL_IRON, 200)), can(130, set_preheat_can(2, TOOL_PRINTER, 150)), tick(180)],
        replies=["applied", "applied"], targets={0: 200, 1: 150}, link_lost=False))
    out.append(Scenario("out_of_range_goes_safe", "a target above the ceiling puts that tool in the safe state and is refused", [*RACK,
        serial(100, set_preheat(1, TOOL_IRON, 200)), serial(200, set_preheat(2, TOOL_IRON, 500))],
        replies=["applied", "temp_out_of_range"], targets={0: 0}, link_lost=False))
    out.append(Scenario("tool_limit", "a tool limited to 310 C refuses 320 C and accepts 310 C", [*RACK,
        serial(100, set_preheat(1, TOOL_LASER_ISH, 320)), serial(200, set_preheat(2, TOOL_LASER_ISH, 310))],
        replies=["temp_out_of_range", "applied"], targets={2: 310}))
    out.append(Scenario("not_heatable", "a target above zero for a tool with no heater is refused", [*RACK, slot(0, 3, TOOL_NO_HEATER, 4),
        serial(100, set_preheat(1, TOOL_NO_HEATER, 100)), serial(200, set_preheat(2, TOOL_NO_HEATER, 0))],
        replies=["not_heatable", "applied"], targets={3: 0}))
    out.append(Scenario("retransmission", "the same sequence twice is applied once and acknowledged twice", [*RACK,
        serial(100, set_preheat(7, TOOL_IRON, 200)), serial(150, set_preheat(7, TOOL_IRON, 200))],
        replies=["applied", "repeat"], targets={0: 200}))
    out.append(Scenario("rate_limit", "a second command for the same tool within 20 ms is held and changes nothing", [*RACK,
        serial(100, set_preheat(1, TOOL_IRON, 200)), serial(105, set_preheat(2, TOOL_IRON, 210)), serial(130, set_preheat(2, TOOL_IRON, 210))],
        replies=["applied", "held_rate", "applied"], targets={0: 210}))
    good = set_preheat(1, TOOL_IRON, 200)
    damaged = bytearray(good)
    damaged[-1] ^= 0xFF
    out.append(Scenario("damaged_crc", "a frame with a wrong check byte is ignored, answers nothing and does not revive the link", [*RACK, serial(100, bytes(damaged))],
        replies=[], targets={0: 0}, link_lost=True))
    out.append(Scenario("noise_before_a_frame", "noise in front of a frame is skipped and the frame is applied once", [*RACK,
        serial(100, bytes([0x00, 0x17, 0xA5, 0x03, 0xFF]) + good)], replies=["applied"], targets={0: 200}))
    out.append(Scenario("frame_in_two_pieces", "a frame whose halves come within the timeout is applied", [*RACK, serial(100, good[:4]), serial(110, good[4:])],
        replies=["applied"], targets={0: 200}))
    out.append(Scenario("frame_cut_short", "a frame whose second half comes after the timeout is not applied", [*RACK, serial(100, good[:4]), serial(400, good[4:]), tick(450)],
        targets={0: 0}))
    out.append(Scenario("unknown_command", "a well-formed frame with a command the rack does not have is told so and changes nothing", [*RACK,
        serial(100, frame(1, 0x55, b"\x00")), tick(120)], replies=["unknown_command"], targets={0: 0, 1: 0, 2: 0}))
    out.append(Scenario("empty_slot", "a target for a tool that is not racked is acknowledged and applied nowhere", [slot(0, 0, TOOL_IRON, 1), serial(100, set_preheat(1, TOOL_PRINTER, 100))],
        replies=["applied"], targets={0: 0}))
    out.append(Scenario("link_loss_and_return", "after 250 ms of silence every target goes to the safe state; the sequence may start again when the link returns", [*RACK,
        serial(100, set_preheat(1, TOOL_IRON, 200)), tick(300), tick(400), serial(500, set_preheat(1, TOOL_IRON, 180)), tick(520)],
        replies=["applied", "applied"], targets={0: 180}, link_lost=False))
    out.append(Scenario("link_loss_stays", "after the silence the targets stay at the safe state", [*RACK, serial(100, set_preheat(1, TOOL_IRON, 200)), tick(400), tick(2000)],
        replies=["applied"], targets={0: 0}, link_lost=True))
    out.append(Scenario("wrong_length", "a command with the wrong payload length is refused", [*RACK, serial(100, frame(1, CMD_SET_PREHEAT, bytes([TOOL_IRON, 0])))],
        replies=["wrong_length"]))
    out.append(Scenario("wrong_can_frame", "a CAN frame with another identifier or length is not a rack frame and is ignored", [*RACK,
        can(100, set_preheat_can(1, TOOL_IRON, 200), 0x261), can(120, set_preheat_can(1, TOOL_IRON, 200)[:5])], replies=[], targets={0: 0}, link_lost=True))
    out.append(preheat_limit_scenario())
    return out


def preheat_limit_scenario() -> Scenario:
    """A pre-heat that is never ended stops after ten minutes, a new target waits for a 0, and then the tool heats again."""
    events: list[Event] = [*RACK, serial(100, set_preheat(1, TOOL_IRON, 200))]
    seq = 1
    at = 100
    while at + 200 <= PREHEAT_MAX_MS + 100:                  # the printer keeps the link alive with a zero target every 200 ms
        at += 200
        seq = (seq + 1) & 0xFF
        events.append(can(at, set_preheat_can(seq, TOOL_PRINTER, 0)))
    events.append(tick(PREHEAT_MAX_MS + 100))
    events.append(can(PREHEAT_MAX_MS + 150, set_preheat_can(2, TOOL_IRON, 150)))          # refused by the limit: the slot waits for a 0
    events.append(can(PREHEAT_MAX_MS + 200, set_preheat_can(3, TOOL_IRON, 0)))            # the 0 clears the stop
    events.append(can(PREHEAT_MAX_MS + 250, set_preheat_can(4, TOOL_IRON, 120)))          # and the tool heats again
    expected = ["applied"] * (len(events) - len(RACK) - 1)
    return Scenario("preheat_limit", "ten minutes of pre-heat end by themselves; a new target waits for a 0", events, replies=expected, targets={0: 120},
                    invariant=expired_then_cleared(PREHEAT_MAX_MS + 100, PREHEAT_MAX_MS + 150, PREHEAT_MAX_MS + 200))


def expired_then_cleared(stop_at: int, refused_at: int, cleared_at: int) -> Callable[[list[Step]], list[str]]:
    def check(steps: list[Step]) -> list[str]:
        problems: list[str] = []
        stop_index = len(steps) - 4            # the four last events are: the tick, the refused target, the zero, the new target (see preheat_limit_scenario)
        if steps[stop_index].slots[0][3] != 0 or steps[stop_index].slots[0][4] != 1:
            problems.append(f"at {stop_at} ms the iron must be stopped and marked as ran out, found {steps[stop_index].slots[0]}")
        if steps[stop_index + 1].slots[0][3] != 0:
            problems.append(f"at {refused_at} ms a new target must wait for a 0, found {steps[stop_index + 1].slots[0]}")
        if steps[stop_index + 2].slots[0][4] != 0:
            problems.append(f"at {cleared_at} ms the 0 must clear the stop, found {steps[stop_index + 2].slots[0]}")
        return problems
    return check


def random_scenario(seed: int, count: int) -> Scenario:
    """Seeded random run: commands for three tools over both links with faults injected (damaged, repeated, cut, noisy, late, lost), clock moving forward."""
    rng = random.Random(seed)
    events: list[Event] = list(RACK)
    at = 10
    seq = {TOOL_IRON: 0, TOOL_PRINTER: 0, TOOL_LASER_ISH: 0}
    for _ in range(count):
        at += rng.choice([1, 5, 10, 19, 20, 21, 40, 120, 240, 260, 700])
        tool = rng.choice([TOOL_IRON, TOOL_PRINTER, TOOL_LASER_ISH, TOOL_NO_HEATER, 5, 30, 31])
        seq[tool] = (seq.get(tool, 0) + rng.choice([1, 1, 1, 0, 2, 250])) & 0xFF
        temp = rng.choice([0, 0, 100, 180, 250, 310, 311, 399, 400, 401, 1000])
        wire = rng.choice(["serial", "serial", "can", "burst"])
        data = set_preheat(seq[tool], tool, temp) if wire != "can" else set_preheat_can(seq[tool], tool, temp)
        fault = rng.choice(["none"] * 6 + ["damage", "repeat", "cut", "noise", "lose", "late"])
        if fault == "lose":
            events.append(tick(at))
            continue
        if fault == "damage":
            damaged = bytearray(data)
            damaged[rng.randrange(len(damaged))] ^= 1 << rng.randrange(8)
            data = bytes(damaged)
        elif fault == "cut" and wire != "can":
            data = data[: rng.randrange(1, len(data))]
        elif fault == "noise" and wire != "can":
            data = bytes(rng.randrange(256) for _ in range(rng.randrange(1, 6))) + data
        if wire == "can":
            events.append(can(at, data))
            if fault == "repeat":
                events.append(can(at + 1, data))
        else:
            events.append(burst(at, data) if wire == "burst" else serial(at, data))
            if fault == "repeat":
                events.append(serial(at + 1, data))
        if fault == "late":
            at += 300
    return Scenario(f"random_{seed}", f"{count} random commands with injected faults (seed {seed})", events, invariant=random_invariants)


CEILING = {TOOL_IRON: 400, TOOL_PRINTER: 400, TOOL_LASER_ISH: 310}


def random_invariants(steps: list[Step]) -> list[str]:
    """What must hold whatever the frames were: no target above the ceiling of its tool, none for a tool that is not racked, none while the link is lost."""
    problems: list[str] = []
    for index, step in enumerate(steps):
        for number, (occupied, tool, _serial, target, _expired) in enumerate(step.slots):
            if target != 0 and (not occupied or target > CEILING.get(tool, 0)):
                problems.append(f"step {index}: slot {number} holds {target} C for tool {tool} (racked {occupied})")
        if step.link_lost and any(entry[3] != 0 for entry in step.slots) and index > len(RACK):
            problems.append(f"step {index}: a target is set while the link is lost")
        if len(problems) >= 5:
            break
    return problems


def check_expectations(scenario: Scenario, steps: list[Step]) -> list[str]:
    problems: list[str] = []
    if scenario.replies is not None:
        got = [reason_of(reply) for step in steps for reply in step.replies]
        if got != scenario.replies:
            problems.append(f"{scenario.name}: replies {got} must be {scenario.replies}")
    if scenario.targets:
        for number, target in scenario.targets.items():
            if steps[-1].slots[number][3] != target:
                problems.append(f"{scenario.name}: slot {number} ends at {steps[-1].slots[number][3]} C and must end at {target} C")
    if scenario.link_lost is not None and steps[-1].link_lost != scenario.link_lost:
        problems.append(f"{scenario.name}: the link ends lost={steps[-1].link_lost} and must be lost={scenario.link_lost}")
    if scenario.invariant is not None:
        problems.extend(f"{scenario.name}: {message}" for message in scenario.invariant(steps))
    return problems


def burst_overflow_check(prefix: list[str]) -> list[str]:
    """Four hundred bytes arriving before the main loop polls: the ring keeps what fits, the loss is counted, nothing becomes a target by accident."""
    noise = bytes((index * 37) & 0xFF for index in range(400))
    steps = play(prefix, [*RACK, burst(100, noise), serial(200, set_preheat(1, TOOL_IRON, 150))])
    problems: list[str] = []
    if steps[-2].dropped == 0:
        problems.append("burst_overflow: 400 bytes at once must overflow the ring and be counted")
    if steps[-1].slots[0][3] != 150:
        problems.append(f"burst_overflow: a good command after the burst must still be applied, slot 0 is at {steps[-1].slots[0][3]} C")
    return problems


def run_all(prefix: list[str], rack_sim: Any, only: list[str], seed: int, count: int) -> list[str]:
    scenarios = [*fixed_scenarios(), random_scenario(seed, count), random_scenario(seed + 1, count)]
    problems: list[str] = []
    ran = 0
    for scenario in scenarios:
        if only and scenario.name not in only:
            continue
        ran += 1
        steps = play(prefix, scenario.events)
        found = check_expectations(scenario, steps)
        if rack_sim is not None:
            found.extend(compare(scenario.name, steps, model_steps(rack_sim, scenario.events), scenario.events))
        print(f"RACK_BENCH {scenario.name}: {'FAIL' if found else 'PASS'} ({len(scenario.events)} events{', compared with the model' if rack_sim is not None else ''})")
        problems.extend(found)
    if not only or "burst_overflow" in only:
        ran += 1
        found = burst_overflow_check(prefix)
        print(f"RACK_BENCH burst_overflow: {'FAIL' if found else 'PASS'}")
        problems.extend(found)
    if ran == 0:
        problems.append("no scenario has that name")
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Play scenarios with faults through the real rack logic compiled for the host.")
    parser.add_argument("--list", action="store_true", help="list the scenarios")
    parser.add_argument("--only", action="append", default=[], help="run only this scenario (repeatable)")
    parser.add_argument("--seed", type=int, default=4242, help="seed of the random scenarios")
    parser.add_argument("--events", type=int, default=1500, help="commands in each random scenario")
    parser.add_argument("--dump", metavar="DIR", help="write the script of every scenario to DIR")
    parser.add_argument("--replay", metavar="FILE", help="play a recorded script through the virtual rack and print its answers")
    parser.add_argument("--no-model", action="store_true", help="do not compare with the Python model")
    parser.add_argument("--common", metavar="DIR", help="folder that holds the urtc_common package (URTC-COMMON/src)")
    parser.add_argument("--build-dir", metavar="DIR", help="where the binary goes (default: a temporary folder)")
    args = parser.parse_args(argv)

    if args.list:
        for scenario in [*fixed_scenarios(), random_scenario(args.seed, args.events)]:
            print(f"{scenario.name}: {scenario.about}")
        print("burst_overflow: more bytes than the receive ring holds arrive before the main loop polls")
        return 0
    if args.dump:
        target = Path(args.dump)
        target.mkdir(parents=True, exist_ok=True)
        for scenario in [*fixed_scenarios(), random_scenario(args.seed, args.events)]:
            (target / f"{scenario.name}.txt").write_text(script_of(scenario.events), encoding="utf-8", newline="\n")
        print(f"RACK_BENCH_DUMP={target}")
        return 0

    with tempfile.TemporaryDirectory(dir=args.build_dir) as directory:
        prefix = build(Path(directory) / "virtual_rack")
        if prefix is None:
            print("RACK_BENCH=SKIPPED no host C compiler, or local builds are locked (ALLOW_LOCAL_BUILD=1 allows them)")
            return 2
        if args.replay:
            done = subprocess.run(prefix, input=Path(args.replay).read_text(encoding="utf-8"), capture_output=True, text=True, timeout=300, check=False)
            sys.stdout.write(done.stdout)
            return done.returncode
        rack_sim = None if args.no_model else import_model(args.common)
        if rack_sim is None and not args.no_model:
            print("RACK_BENCH note: urtc_common is not importable, so the replies are checked against the scenarios only (set URTC_COMMON_SRC)")
        problems = run_all(prefix, rack_sim, args.only, args.seed, args.events)
    for problem in problems:
        print(f"RACK_BENCH problem: {problem}")
    print("RACK_BENCH=PASS" if not problems else f"RACK_BENCH=FAIL {len(problems)} problem(s)")
    return 0 if not problems else 1


if __name__ == "__main__":
    raise SystemExit(main())
