# EarthBound Switch — vanilla QoL development build

This is the **original native EarthBound engine**, not the ShrineFox SNES ROM patch. The
ShrineFox project informed the requested feature set:
https://github.com/ShrineFox/EarthBound-Mod-Menu

## Controls

- Open/close the native QoL menu: **ZL + PLUS**, or **left-stick click + PLUS**
- Navigate: D-pad
- Activate an option: A
- Close: B
- **Hold Y**: minimum text typewriter delay (does not accelerate the 60-Hz engine)

## Implemented in this branch

- A dark, Switch-resolution overlay drawn through SDL (no external font dependency)
- Display scaling selection (nearest, integer, linear), saved to `scaling.cfg` on SD
- Quick text toggle; hold-Y quick text
- Disable periodic Dad reminder calls
- Prevent new homesickness rolls (does not heal existing homesickness)
- Auto-heal, heal party immediately, add $1000
- Persist switches in `sdmc:/switch/earthbound-switch/qol.cfg`

## Not implemented — do not advertise as present

- Running/fast movement (needs movement-state-aware implementation to avoid staircase softlocks)
- Key Items partition, Jeff Tools menu, equip-from-Goods (all require inventory/serialization work)
- Fast doors, swims, climbing, cutscene skipping, photo shortener, level-up text skipping
- No-crying miss mechanics, PSI -> PK localization (requires script/text and battle modification)
- Full party editing, Chaos mode, unrestricted bike, difficulty modes
- Unique overworld sprites, animated battle sprites and MOTHER 3-style party portraits

These must be individually implemented and regression-tested. Native C code cannot
load a SNES IPS/BPS patch as gameplay logic.

## Testing and safety

1. **Back up `earthbound.srm` before trying cheats.**
2. Build on macOS with the existing devkitPro environment: `cd port/switch && make`.
3. Test title, menu, first story area, game save/reload, party healing and scaling.
4. Preserve the previously working `.nro` for recovery.

The overlay has not been validated on Switch hardware as of this change.
