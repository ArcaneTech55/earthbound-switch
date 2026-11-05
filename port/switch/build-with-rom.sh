#!/usr/bin/env bash
# Build an EarthBound Switch NRO from your own USA ROM, WITHOUT publishing it.
# Usage: bash port/switch/build-with-rom.sh /path/to/EarthBound-USA.sfc
set -euo pipefail

if [ "$#" -ne 1 ] || [ ! -f "$1" ]; then
  echo "Usage: bash port/switch/build-with-rom.sh /path/to/EarthBound-USA.sfc" >&2
  exit 2
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ROM="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
if ! command -v docker >/dev/null 2>&1; then
  echo "Docker is required (official devkitPro image)." >&2
  exit 2
fi

# Bind mount a LOCAL ROM outside the repository as read-only. Nothing here
# commits, publishes, uploads, or adds the game to GitHub.
docker run --rm --platform linux/amd64 \
  -v "$ROOT:/src" \
  -v "$ROM:/private/earthbound.sfc:ro" \
  -w /src \
  devkitpro/devkita64 \
  bash -euc '
    git submodule update --init --recursive
    # The official devkitA64 image has Python but not necessarily pip/venv.
    # Install host-side asset tooling only inside this disposable container.
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends python3-venv python3-pip
    python3 -m venv /tmp/eb-tools-venv
    /tmp/eb-tools-venv/bin/python -m pip install -e .
    export PATH="/tmp/eb-tools-venv/bin:$PATH"
    ebtools extract earthbound.yml /private/earthbound.sfc
    cmake -S port/switch -B port/switch/build \
      -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
      -DCMAKE_BUILD_TYPE=Release \
      -DEB_SWITCH_TRUE_WIDESCREEN=ON
    cmake --build port/switch/build -j 4
    test -s port/switch/build/EarthboundSwitch.nro
    echo "Built: /src/port/switch/build/EarthboundSwitch.nro"
  '

echo "Local NRO: $ROOT/port/switch/build/EarthboundSwitch.nro"
echo "Copy to sdmc:/switch/earthbound-switch/EarthboundSwitch.nro"
