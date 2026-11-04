#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$ROOT/companion-switch/.work"
if [ -e "$WORK/companion" ] || [ -e "$WORK/native" ]; then
  echo "Refusing to overwrite existing source. Use a clean working directory." >&2
  exit 1
fi
mkdir -p "$WORK"
git clone https://github.com/rages4calm/earthbound-companion.git "$WORK/companion"
python3 "$WORK/companion/scripts/prepare_port_sources.py" --native "$WORK/native"
echo "Prepared Companion C engine: $WORK/native"
echo "This step does NOT compile a Switch NRO."
