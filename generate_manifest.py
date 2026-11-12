#!/usr/bin/env python3
# =============================================================================
# generate_manifest.py - Write firmware/firmware_manifest.json for URTC-SMART-RACK
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D) <electrohobby3d@gmail.com>
# GPL-3.0-or-later - see LICENSE
# =============================================================================
"""Indexes the firmware image of the version in the source: its file names, sizes, CRC32 and SHA-256.

The version is read from src/firmware_common.h (never written here) and only the files of exactly that version are indexed, so an older image that
lingers in firmware/ never ends up in the manifest. Run it after a build, or any time firmware/ changes:

    python3 generate_manifest.py [repo_root [firmware_dir]]

repo_root defaults to the folder of this script and firmware_dir to <repo_root>/firmware (a CI run can point it at build/ so tracked files stay untouched).
The time written in the manifest is reproducible: SOURCE_DATE_EPOCH if set, else (with URTC_CI=1) the date of the last commit, else the current time.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import sys
import zlib
from datetime import datetime, timezone
from pathlib import Path

PROJECT = "URTC-SMART-RACK"
PREFIX = "URTC_SMART_RACK_FIRMWARE"
SCHEMA = "urtc.firmware.manifest/1"
MANIFEST_NAME = "firmware_manifest.json"


def build_timestamp(root: Path, environ: dict | None = None) -> str:
    environ = os.environ if environ is None else environ
    epoch = environ.get("SOURCE_DATE_EPOCH", "").strip()
    if not epoch and environ.get("URTC_CI") == "1":
        try:
            epoch = subprocess.run(["git", "log", "-1", "--format=%ct"], cwd=root, capture_output=True, text=True, check=True).stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            epoch = ""
    if epoch:
        try:
            moment = datetime.fromtimestamp(int(epoch), timezone.utc)
        except (ValueError, OverflowError, OSError):
            raise SystemExit(f"generate_manifest.py: invalid timestamp source {epoch!r}") from None
    else:
        moment = datetime.now(timezone.utc)
    return moment.strftime("%Y-%m-%dT%H:%M:%SZ")


def read_version(header: Path) -> str:
    """MAJOR.MINOR.PATCH from the FIRMWARE_VERSION_* defines of the header."""
    text = header.read_text(encoding="utf-8")
    parts = []
    for name in ("MAJOR", "MINOR", "PATCH"):
        match = re.search(rf"(?m)^#define\s+FIRMWARE_VERSION_{name}\s+(\d+)", text)
        if not match:
            raise SystemExit(f"generate_manifest.py: no #define FIRMWARE_VERSION_{name} in {header} - manifest NOT written")
        parts.append(match.group(1))
    return ".".join(parts)


def file_entry(path: Path, with_crc: bool) -> dict:
    data = path.read_bytes()
    entry = {"filename": path.name, "size_bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
    if with_crc:
        entry["crc32"] = f"0x{zlib.crc32(data) & 0xFFFFFFFF:08X}"
    return entry


def build_manifest(root: Path, firmware_dir: Path, environ: dict | None = None) -> dict:
    version = read_version(root / "src" / "firmware_common.h")
    stem = f"{PREFIX}_v{version}"
    binary = firmware_dir / (stem + ".bin")
    if not binary.is_file():
        raise SystemExit(f"generate_manifest.py: {binary.name} not found in {firmware_dir} - run a full build first, manifest NOT written")
    files = {"bin": file_entry(binary, True)}
    for extension in ("hex", "elf"):
        path = firmware_dir / (stem + "." + extension)
        files[extension] = file_entry(path, False) if path.is_file() else None
    return {"schema": SCHEMA, "project": PROJECT, "version": version, "generated_at": build_timestamp(root, environ), "files": files}


def main(argv: list[str]) -> int:
    root = Path(argv[0]).resolve() if argv else Path(__file__).resolve().parent
    firmware_dir = Path(argv[1]).resolve() if len(argv) > 1 else root / "firmware"
    manifest = build_manifest(root, firmware_dir)
    target = firmware_dir / MANIFEST_NAME
    target.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"{target} written for v{manifest['version']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
