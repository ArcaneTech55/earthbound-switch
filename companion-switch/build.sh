#!/usr/bin/env bash
# Try compiling the patched Companion engine as a native Nintendo Switch NRO.
# Everything stays under companion-switch/.work/switch-stage/.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE="$ROOT/companion-switch/.work/switch-stage"
OUT="$STAGE/port/switch/build"
LOG="$ROOT/companion-switch/.work/companion-switch-build.log"
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
if [ -f "$ROOT/.venv/bin/activate" ]; then
  source "$ROOT/.venv/bin/activate"
fi
if [ ! -f "$STAGE/port/switch/CMakeLists.txt" ]; then
  python3 "$ROOT/companion-switch/stage.py"
fi
echo "Building actual patched Companion C engine for Switch..."
echo "Build output: $LOG"
echo "Existing EarthBound NRO and saves will not be touched."
set +e
(
  cd "$STAGE/port/switch"
  make -j2
) > >(tee "$LOG") 2>&1
status=$?
set -e
if [ "$status" -ne 0 ]; then
  echo
  echo "Build failed at the first unresolved Companion/Switch incompatibility."
  echo "This is not a playable NRO. Send the last 80 log lines below:"
  tail -n 80 "$LOG"
  exit "$status"
fi
if [ ! -s "$OUT/EarthBoundCompanion.nro" ]; then
  echo "CMake completed without the expected Companion NRO; inspect $LOG"
  exit 1
fi
echo "Built: $OUT/EarthBoundCompanion.nro"
echo "UNTESTED: do not replace your working vanilla game; first validate boot and saves separately."
