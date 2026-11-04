#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$ROOT/companion-switch/.work"
test -f "$WORK/companion/UPSTREAM.md" || { echo "Run bash companion-switch/prepare.sh first"; exit 1; }
test -f "$WORK/native/src/CMakeLists.txt" || { echo "Missing Companion native C core"; exit 1; }
test -f "/opt/devkitpro/cmake/Switch.cmake" || { echo "Missing devkitPro Switch toolchain"; exit 1; }
test -x "/opt/devkitpro/devkitA64/bin/aarch64-none-elf-gcc" || { echo "Missing devkitA64 compiler"; exit 1; }
echo "Companion native sources and Switch compiler present."
echo "Switch backend integration is not complete; no Companion NRO exists yet."
