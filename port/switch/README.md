# EarthBound native Switch / true widescreen development

**Development status: incomplete. Not yet a playable Nintendo Switch app.**

## Goals
- Nintendo Switch homebrew `.nro` built with devkitPro/libnx
- Genuine 16:9 scenes at a 1280×720 output resolution (400×225 logical layout)
- Preserve sprite pixel proportions; expose more overworld world-space tiles
- Extend the title, battles, PSI animations, dialogue, menus, and all cutscenes
- Use newly authored 16:9 assets where the original graphics cannot be extended
- Preserve saves, audio, and portable controller support

## Implemented in this branch
- `widescreen/layout.[ch]`: layout, aspect-fit, anchors and negative world-tile division
- `input_mapping.[ch]`: Nintendo-labeled buttons → EarthBound SNES pad bits
- `platform/switch_sdl2_input.c`: SDL2 controller detection, hot-plugging, per-frame sampling
- Standalone host-side regression tests for both the layout and controller mapper

The SDL2 driver is **a platform module, not yet built into a playable game**.
Its button labels and hot-plug behavior need real Switch hardware validation.

## Host tests
```sh
cmake -S port/switch/widescreen -B build/switch-host-tests
cmake --build build/switch-host-tests
ctest --test-dir build/switch-host-tests --output-on-failure
```

## Next milestones
1. Wire the Switch platform backend to `src/platform/platform.h` using devkitPro/libnx,
   with a working game loop, video, audio, and save storage.
2. Confirm native gameplay on Switch at the original viewport without aspect distortion.
3. Expand the game renderer to draw additional world-space scenery in a 16:9 viewport.
4. Rebuild the title, battle backgrounds, UI, PSI, cinematics, and endings for widescreen.
5. Test on real hardware and package a release `.nro`.

Simply setting `EB_VIEWPORT_WIDTH=400` or stretching 256×224 is **not**
equivalent to implementing this project's true widescreen renderer.

## Copyright and privacy
Game assets are extracted locally from a legally obtained personal ROM. Do
not commit `.sfc`, `.smc`, extracted game data, or save files to this
public GitHub repository. The base project's `.gitignore` already excludes
`*.sfc` and common extracted output directories.
