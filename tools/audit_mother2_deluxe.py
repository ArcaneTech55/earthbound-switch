#!/usr/bin/env python3
"""Validate private EarthBound/Deluxe ROMs and locate changed 32-KiB banks.

Produces hashes and byte counts only, NEVER ROM contents or a game executable.
This does not port Deluxe's SNES assembly code to C.
"""
import argparse
import hashlib
import json
from pathlib import Path
import zlib

VANILLA_CRC = 0xDC9BB451
DELUXE_SHA256 = "635b4a148cfd1abf8cb43b37726b094183a81a92d11043090d7df1fc391a0f1a"
BANK_SIZE = 0x8000

def inspect(vanilla, deluxe):
    original = Path(vanilla).read_bytes()
    patched = Path(deluxe).read_bytes()
    if len(original) != 0x300000 or zlib.crc32(original) != VANILLA_CRC:
        raise ValueError("Need the headerless 3 MiB EarthBound (USA) ROM, CRC32 DC9BB451")
    if len(patched) != 0x600000 or hashlib.sha256(patched).hexdigest() != DELUXE_SHA256:
        raise ValueError("This ROM is not the 6 MiB Mother 2 Deluxe v3.0.16 sample")
    banks = []
    for offset in range(0, len(patched), BANK_SIZE):
        chunk = patched[offset:offset+BANK_SIZE]
        base = original[offset:offset+BANK_SIZE] if offset < len(original) else None
        banks.append({
            "offset": f"0x{offset:06x}",
            "region": "vanilla_bank" if base is not None else "expanded_bank",
            "sha256": hashlib.sha256(chunk).hexdigest(),
            "nonzero_bytes": sum(x != 0 for x in chunk),
            "changed_bytes": sum(x != y for x,y in zip(base,chunk)) if base is not None else None,
        })
    changed = sum(x != y for x,y in zip(original,patched))
    return {
        "status": "analysis_only_not_a_playable_port",
        "vanilla_sha256": hashlib.sha256(original).hexdigest(),
        "deluxe_sha256": hashlib.sha256(patched).hexdigest(),
        "changed_original_bytes": changed,
        "changed_original_percent": round(changed / len(original)*100,2),
        "expanded_nonzero_bytes": sum(x!=0 for x in patched[len(original):]),
        "bank_size": BANK_SIZE,
        "banks": banks,
    }

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("vanilla")
    parser.add_argument("deluxe")
    parser.add_argument("--out", default="mother2_deluxe_bank_audit.json")
    args = parser.parse_args()
    report = inspect(args.vanilla,args.deluxe)
    Path(args.out).write_text(json.dumps(report,indent=2)+"\n")
    print("Validated ROMs; analyzed",len(report["banks"]),"banks. Wrote",args.out)

if __name__ == "__main__":
    main()
