# EarthBound Switch — native port (experimental)

**Status: first-pass executable scaffolding, NOT compiled or hardware-verified.**
No playable release is available yet.

## Already implemented on the development branch

- `main.c`: libnx SD mount, SDL init, game loop, lifecycle/cleanup
- `platform/switch_sdl2_video.c`: scanline BGR565 texture upload, renderer,
  correct aspect-fit center placement on a 1280x720 display (temporary borders,
  NOT fake widescreen)
- `platform/switch_sdl2_input.c`: SDL2 controller state, hotplug, Nintendo labels
- Reuses the existing desktop SDL audio/timer and file-backed SRAM implementations
- `platform/switch_debug.c`: early-port debug no-ops
- `widescreen/layout.[ch]`: 400x225 logical 16:9 layout foundation,
  rect placement and signed tile-grid math
- Tests for input mapping and widescreen presentation math

The current executable configuration **intentionally uses 256x224** because
the actual world, UI, battle, and cutscene renderers are not widescreen-ready.
It preserves the source geometry rather than stretching it.

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
