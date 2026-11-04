#!/usr/bin/env python3
"""Package an experimental Companion Switch build only when its assets match."""
from pathlib import Path
import argparse
import re
import shutil
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "companion-switch" / ".work"
STAGE = WORK / "switch-stage"
NRO = STAGE / "port" / "switch" / "build" / "EarthBoundCompanion.nro"
EXPECTED = STAGE / "src" / "data" / "runtime_generated" / "asset_pack_layout.h"

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--assets", type=Path, default=WORK / "assets.pak",
                   help="Companion assets.pak generated for this engine version")
    p.add_argument("--destination", type=Path,
                   default=Path.home() / "Documents" / "EarthBound Companion Switch")
    args = p.parse_args()
    if not NRO.is_file() or NRO.stat().st_size < 1024:
        p.error(f"Compiled NRO missing or unusually small: {NRO}")
    if not EXPECTED.is_file():
        p.error("Generated engine asset layout header missing; stage/build first")
    match = re.search(r'ASSET_PACK_LAYOUT_HASH\s+"([0-9a-f]{64})"', EXPECTED.read_text())
    if not match:
        p.error("Could not read the engine asset layout hash")
    pack = args.assets.expanduser()
    if not pack.is_file():
        p.error(f"assets.pak missing: {pack} (run ebtools setup first)")
    with pack.open("rb") as f:
        header = f.read(44)
        if len(header) != 44 or header[:4] != b"EBPK":
            p.error("Invalid assets.pak header (expected EBPK)")
        version, count = struct.unpack_from("<II", header, 4)
        if version != 1 or count == 0:
            p.error(f"Incompatible assets.pak version/count: {version}/{count}")
        actual = header[12:44].hex()
    if actual != match.group(1):
        p.error("assets.pak layout mismatch! Rebuild the pak using the same "
                "Companion native source revision; do not deploy an incompatible pack.")
    dest = args.destination.expanduser()
    dest.mkdir(parents=True, exist_ok=True)
    shutil.copy2(NRO, dest / NRO.name)
    shutil.copy2(pack, dest / "assets.pak")
    zipped = dest.parent / "EarthBoundCompanion-Switch.zip"
    with zipfile.ZipFile(zipped, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=6) as z:
        z.write(NRO, "switch/earthbound-companion/EarthBoundCompanion.nro")
        z.write(pack, "switch/earthbound-companion/assets.pak")
    print("Validated Companion assets.pak header and layout hash.")
    print(f"Switch-ready folder: {dest}")
    print(f"SD-root ZIP: {zipped}")
    print("Unzip at the SD card root (not inside another /switch folder).")
    print("Hardware boot not verified; if Homebrew Menu returns, retrieve "
          "/switch/earthbound-companion/startup.log from your SD card.")

if __name__ == "__main__":
    main()
