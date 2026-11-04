#!/usr/bin/env python3
"""Stage only byte-identical EarthBound asset ranges from a private Deluxe ROM.

An unchanged compressed or raw range is a candidate for reuse by the native
asset pipeline. Changed/expanded ranges are NEVER passed to vanilla tooling
without manual format verification. ROM bytes stay local and are not committed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import zlib

import yaml

VANILLA_CRC32 = 0xDC9BB451
DELUXE_SHA256 = "635b4a148cfd1abf8cb43b37726b094183a81a92d11043090d7df1fc391a0f1a"


def stage(vanilla: Path, deluxe: Path, layout: Path, out: Path) -> dict:
    src = vanilla.read_bytes()
    mod = deluxe.read_bytes()
    if len(src) != 0x300000 or zlib.crc32(src) != VANILLA_CRC32:
        raise ValueError("Expected headerless EarthBound USA, CRC32 DC9BB451")
    if len(mod) != 0x600000 or hashlib.sha256(mod).hexdigest() != DELUXE_SHA256:
        raise ValueError("Expected Mother 2 Deluxe v3.0.16, 6 MiB, verified SHA256")
    spec = yaml.safe_load(layout.read_text())
    rows = []
    out.mkdir(parents=True, exist_ok=True)
    seen = set()
    for entry in spec.get("dumpEntries", []):
        offset = entry.get("offset")
        length = entry.get("size")
        folder = entry.get("subdir", "")
        filename = str(entry.get("name", ""))
        ext = str(entry.get("extension", "bin"))
        if not isinstance(offset, int) or not isinstance(length, int) or length < 0:
            continue  # variable-length/custom extractor: never guess boundaries
        if offset < 0 or offset + length > len(src):
            continue
        if not filename or "/" in filename or "\\" in filename:
            continue
        rel = Path(folder) / (filename + "." + ext)
        if rel.is_absolute() or ".." in rel.parts:
            continue
        if str(rel) in seen:
            continue
        seen.add(str(rel))
        unchanged = src[offset:offset+length] == mod[offset:offset+length]
        row = {
            "asset": rel.as_posix(), "rom_offset": hex(offset), "size": length,
            "status": "byte_identical_candidate" if unchanged else "deluxe_changed_requires_port",
        }
        rows.append(row)
        if unchanged:
            path = out / "unchanged" / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(mod[offset:offset+length])
    report = {
        "warning": "Asset staging only. Does not recompile Deluxe gameplay or make a playable Deluxe NRO.",
        "deluxe_sha256": DELUXE_SHA256,
        "layout": str(layout),
        "eligible_asset_ranges": len(rows),
        "unchanged_candidates": sum(r["status"] == "byte_identical_candidate" for r in rows),
        "changed_requires_port": sum(r["status"] == "deluxe_changed_requires_port" for r in rows),
        "assets": rows,
    }
    (out / "compatibility.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("vanilla", type=Path)
    ap.add_argument("deluxe", type=Path)
    ap.add_argument("--layout", type=Path, default=Path("earthbound.yml"))
    ap.add_argument("--out", type=Path, default=Path("build/deluxe-staging"))
    args = ap.parse_args()
    result = stage(args.vanilla, args.deluxe, args.layout, args.out)
    print("Unchanged candidates:", result["unchanged_candidates"])
    print("Changed ranges requiring port:", result["changed_requires_port"])
    print("Report:", args.out / "compatibility.json")


if __name__ == "__main__":
    main()
