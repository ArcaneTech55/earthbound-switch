#!/usr/bin/env bash
set -euo pipefail
# Native snesrev/smw Switch baseline. Never edits the EarthBound source.
# Usage: bash super-mario-world-switch/build-switch.sh "/path/to/Super Mario World (USA).sfc"

if [[ $# -ne 1 || ! -f "$1" ]]; then
  echo 'Usage: bash super-mario-world-switch/build-switch.sh "/path/to/Super Mario World (USA).sfc"' >&2
  exit 2
fi
ROM="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$SCRIPT_DIR/.work"
SMW="$WORK/smw"
OUT="$WORK/output"
REPO_REV=eae20c65c58930c8b62c76188d259579ad4130f1
EXPECTED_SHA1=6b47bb75d16514b6a476aa0c73a683a2a4c18765

for command in git python3 make shasum; do
  command -v "$command" >/dev/null || { echo "Missing tool: $command" >&2; exit 1; }
done
ROM_SHA1="$(shasum -a 1 "$ROM" | awk '{print $1}')"
if [[ "$ROM_SHA1" != "$EXPECTED_SHA1" ]]; then
  echo "Unsupported ROM SHA1: $ROM_SHA1" >&2
  echo "Expected original unheadered Super Mario World (USA): $EXPECTED_SHA1" >&2
  exit 1
fi

export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
if [[ ! -f "$DEVKITPRO/libnx/switch_rules" ]]; then
  echo "Switch toolchain not found at $DEVKITPRO. Install devkitPro switch-dev first." >&2
  exit 1
fi
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
export PATH="$DEVKITPRO/tools/bin:$DEVKITA64/bin:$DEVKITPRO/portlibs/switch/bin:$PATH"

mkdir -p "$WORK"
if [[ ! -d "$SMW/.git" ]]; then
  git clone --recursive https://github.com/snesrev/smw.git "$SMW"
fi
git -C "$SMW" fetch origin "$REPO_REV"
git -C "$SMW" checkout --detach "$REPO_REV"
git -C "$SMW" submodule update --init --recursive
cp "$ROM" "$SMW/smw.sfc"

echo "Extracting private game assets from your verified ROM..."
(cd "$SMW" && python3 assets/restool.py -r smw.sfc --extract-from-rom)
test -s "$SMW/smw_assets.dat" || { echo "Asset extraction failed" >&2; exit 1; }

# IMPORTANT: native 4:3 game content, not falsely stretched to 16:9.
# The Switch outputs HD 720p/1080p; texture filtering stays pixel sharp.
python3 - "$SMW/src/platform/switch/smw.ini" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1])
s=p.read_text()
section=s.index("[Graphics]")
stop=s.find("\n[",section+1)
if stop<0: stop=len(s)
a=s[section:stop]
for key,value in (
    ("WindowSize","1280x720"),
    ("Fullscreen","1"),
    ("IgnoreAspectRatio","0"),
    ("WindowScale","1"),
    ("NewRenderer","1"),
    ("NoSpriteLimits","1"),
    ("OutputMethod","SDL"),
    ("LinearFiltering","0"),
):
    import re
    rx=re.compile(r"(?m)^\s*#?\s*"+re.escape(key)+r"\s*=.*$")
    if rx.search(a): a=rx.sub(f"{key} = {value}",a,count=1)
    else: a+=f"\n{key} = {value}"
s=s[:section]+a+s[stop:]
p.write_text(s)
PY

echo "Building snesrev native Switch NRO..."
make -C "$SMW/src/platform/switch" -j4
NRO="$SMW/src/platform/switch/smw.nro"
test -s "$NRO" || { echo "Build completed without smw.nro" >&2; exit 1; }
mkdir -p "$OUT/switch/smw"
cp "$NRO" "$OUT/switch/smw/smw.nro"
cp "$SMW/src/platform/switch/smw.ini" "$OUT/switch/smw/smw.ini"
cp "$SMW/smw_assets.dat" "$OUT/switch/smw/smw_assets.dat"
(cd "$OUT" && zip -qr ../SMW-Switch-HD-baseline.zip switch)
echo "NRO: $OUT/switch/smw/smw.nro"
echo "SD zip: $WORK/SMW-Switch-HD-baseline.zip"
echo "NOTE: baseline keeps original game 4:3 content. True expanded widescreen is not implemented."
