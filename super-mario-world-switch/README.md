# Super Mario World — native Switch HD baseline (experimental)

This is an **isolated SMW build workspace**, not a modification of EarthBound.
It starts from `snesrev/smw` at pinned commit `eae20c65c58930c8b62c76188d259579ad4130f1`. The project already includes a native libnx/SDL2 port. Its asset extraction requires a user-provided original Super Mario World (USA) ROM.

## What is implemented

- Repeatable Mac/devkitPro Switch build of the upstream native SMW port
- Validation of the original unheadered USA ROM's SHA-1
- Private local asset extraction; **no ROM or extracted game assets committed to GitHub**
- HD Switch display configuration with nearest/pixel-sharp rendering
- Packaged SD-root ZIP with `smw.nro`, `smw.ini`, and generated `smw_assets.dat`
- Original engine's gameplay and game saving; no new save UI or Companion modifications

## Widescreen status — NOT DONE

**True 16:9 expanded gameplay is not implemented in this branch.**
The existing Switch port's `IgnoreAspectRatio=1` stretches the image; this kit
instead sets `IgnoreAspectRatio=0` for undistorted graphics. A real widescreen
renderer must expand the camera, level backgrounds, HUD, and enemy spawning.
The modern `mstan/SuperMarioWorldRecomp` has an experimental adaptive
renderer but does not currently ship a maintained Switch build. Do not mistake
output resolution (720p/1080p) for native high-resolution game art.

## Build on Mac (devkitPro already installed)

```bash
git fetch origin feature/smw-switch-hd
git checkout feature/smw-switch-hd
bash super-mario-world-switch/build-switch.sh "/path/to/Super Mario World (USA).sfc"
```

The ROM path must point to your own copy on your Mac. The script places
`super-mario-world-switch/.work/SMW-Switch-HD-baseline.zip` with an SD
`switch/smw/` layout. Extract to the **root of your SD card**. Backup
existing SMW saves before testing. An NRO is not claimed tested until an actual
Switch build and boot are confirmed.

## Future true-widescreen engineering

Port the experimental adaptive BG/HUD/camera renderer from `mstan/SuperMarioWorldRecomp`
or implement equivalent expansion against the snesrev PPU and gameplay engine.
Validation must include enemy spawn/culling, scanline effects, transitions,
castle/ghost-house scenes, level camera boundaries, and handheld/docked output.

Sources: https://github.com/snesrev/smw and https://github.com/mstan/SuperMarioWorldRecomp.
