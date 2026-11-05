# EarthBound Switch — native port (experimental)

**Status: experimental widescreen engine configuration and executable scaffolding, NOT compiled or hardware-verified.**
No playable release is available yet.

## Already implemented on the development branch

- `main.c`: libnx SD mount, SDL init, game loop, lifecycle/cleanup
- `platform/switch_sdl2_video.c`: scanline BGR565 texture upload, renderer,
  correct aspect-fit center placement on a 1280x720 display (temporary borders,
  NOT fake widescreen)
- `platform/switch_sdl2_input.c`: SDL2 controller state, hotplug, Nintendo labels
- Reuses the existing desktop SDL audio/timer and file-backed SRAM implementations
- `platform/switch_debug.c`: early-port debug no-ops
- `widescreen/scene_art.[ch]`: original deterministic procedural extension art for title, battle and selected cinematic gutters, with non-mirrored palette transitions
- `widescreen/scaling.[ch]`: nearest-neighbor aspect-fit, integer pixel scaling and optional linear scaling
- `widescreen/layout.[ch]`: 400x224 logical 16:9 layout foundation,
  rect placement and signed tile-grid math
- Tests for input mapping and widescreen presentation math

The native renderer now defaults to an **experimental 400x224 16:9 viewport**,
leveraging the upstream engine's wide-tilemap and full-coordinate sprite paths.
Disable with `-DEB_SWITCH_TRUE_WIDESCREEN=OFF` to fall back to 256x224.
Not all game scenes and effects have been adapted, so the widescreen output
is **not yet guaranteed to be complete or artifact-free**.

## Requirements (development machine)

- devkitPro / devkitA64 / libnx / Switch SDL2 portlibs
- CMake and a working Switch CMake toolchain (configure CMake for aarch64-none-elf)
- Python 3.10+, the project's `ebtools` package, initialized git submodules
- Your **own** US EarthBound SNES ROM, for local asset extraction

The port relies on the asset-generation infrastructure in `src/CMakeLists.txt`.
From the repository root, initialize the submodules, set up a host Python
environment and run `ebtools extract earthbound.yml donor.sfc` before configuring.
See the root README and `make unix` for details on extraction.

Do **not** add the ROM or extracted copyrighted game assets to GitHub.
The executable's output folder is intended for local builds only.

## Experimental build attempt

Example using a devkitPro installation that provides a Switch CMake toolchain:

```sh
export DEVKITPRO=/opt/devkitpro
export DEVKITA64="$DEVKITPRO/devkitA64"
# First: install switch-dev, install ebtools, and extract your own ROM locally.
cmake -S port/switch -B port/switch/build \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build port/switch/build -j4
```

The exact location of the toolchain file is installation-dependent. This
CMake build configuration is currently **unverified**, and will likely need
linker/toolchain adjustments on a real devkitPro environment. No `.nro`
should be considered playable until it builds and passes hardware smoke tests.

On-device SRAM is intended to be stored at
`sdmc:/switch/earthbound-switch/earthbound.srm`.

## True widescreen next steps

1. Establish an actual successfully linked `.nro` and test title/menu/input/audio/SRAM.
2. Audit and change the PPU/background scanline renderer, overworld camera,
   object culling, tilemap streaming, and scroll bounds for full-width world draw.
3. Create true 16:9 title screen, opening/ending, cutscenes, dialogue/menus,
   battle backgrounds and PSI effects, with authentic pixel-art dimensions.
4. Validate cinematics, battle layout, edge cases, performance, and saves.

The target is still native 1280x720 *true* widescreen; nothing is considered
finished merely because it stretches a smaller source image.

## Resolution scaling modes

Create `sdmc:/switch/earthbound-switch/scaling.cfg` on your SD card.
Put exactly one of the following values on the first line:

- `nearest` (default): preserve aspect ratio, nearest-neighbor filtering.
  With the experimental 400x224 scene, fills 1280x720 (3.2x pixels).
- `integer`: use the largest whole-number scale and center the picture.
  A 400x224 viewport at 1280x720 displays 1200x675 (3x), with borders.
- `linear`: aspect fit with smooth bilinear interpolation (optional, not
  recommended for original pixel-art appearance).

The output display resolution is requested as 1280x720. The scaling
calculation uses the **actual SDL renderer output size**, not a hardcoded
window size, so it can also adapt if the video backend exposes a different
resolution (e.g. 1920x1080 in docked mode). Scaling does **not** create
additional map content; only the logical game viewport changes that.

The title and battle renderer now have *initial procedural horizontal extension*
art. This is not a completed pixel-by-pixel remake: title animations, PSI,
battle windows, cinematics, and full-width bespoke compositions still need
scene-by-scene validation and additional authored assets.

Private-ROM build helper (Docker and official devkitPro image; unverified):
`bash port/switch/build-with-rom.sh /path/to/your/EarthBound-USA.sfc`
This script mounts the ROM read-only, extracts it locally, and attempts to
build `EarthboundSwitch.nro`. Do not commit or upload the ROM.

## Experimental native trainer (ShrineFox-inspired)

This branch implements an **original native C trainer UI** inspired by the
feature set of [ShrineFox/EarthBound-Mod-Menu](https://github.com/ShrineFox/EarthBound-Mod-Menu).
It does not load or execute the original SNES ROM patch, CCScript or 65816 code,
and it does not yet reproduce the entire ShrineFox mod.

While in-game, hold **ZL + Plus** (or left-stick click + Plus) to open/close
the trainer. Use **D-pad Up/Down** to choose, **Left/Right** to adjust,
**A** to activate/increment, and **B** to close.

Current trainer options: character selector, scaling, instant-text setting
(placeholder, not yet wired into text engine), auto-heal, heal party,
add $1000 cash, add $1000 to bank, max HP/PP, level, item ID, give item,
speed and guts, close. Inventory grants use the engine's normal
`give_item_to_specific_character` API. Character stats and inventory
changes can affect saves; back up `earthbound.srm` before testing.
Menu does not implement ShrineFox's teleport, battles, events, chaos mode,
key-item system or music player yet.

**Status:** source changes committed, cross-compilation and physical Switch
testing not yet verified. The trainer does not fix the pre-existing
telephone/save freeze or other unrelated native-port issues.
