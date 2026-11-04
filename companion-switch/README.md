# EarthBound Companion → Switch native port

**Status: first experimental source-level Switch integration and build attempt; no verified playable Companion .nro yet.**

Source: https://github.com/rages4calm/earthbound-companion

Companion has a native C game engine and SDL2 portability adaptations for Linux/macOS/Android, so it is a promising Switch port candidate. Its Windows Forms and Avalonia launchers require replacement with a libnx/SDL2 controller-driven frontend.

## On your Mac

From ~/Documents/earthbound-switch, on branch feature/earthbound-companion-switch-port:

    bash companion-switch/prepare.sh
    bash companion-switch/check.sh
    python3 companion-switch/stage.py
    bash companion-switch/build.sh

Preparation calls Companion's official scripts/prepare_port_sources.py, which clones the pinned native source and applies the ordered Companion C patches. The stage script copies the patched C engine into a separate working tree and attaches the known SDL2/libnx Switch backend, selecting Companion's runtime asset-pack mode. The build script then invokes devkitPro and writes diagnostic output to companion-switch/.work/companion-switch-build.log. Local sources are in companion-switch/.work/, excluded from Git. No game data or ROM is downloaded or uploaded. Do not run twice over existing sources.

## Work remaining before a real NRO

1. Compile the patched Companion native engine with devkitPro, addressing host-specific code.
2. Write the libnx SDL2 platform contract: renderer, gamepad, audio, timers, saves and applet lifecycle.
3. Implement the in-game settings UI (not a Windows Forms or Avalonia launcher).
4. Integrate locally prepared assets.pak for Original, then separately add MaternalBound Redux.
5. Verify title, story, controller, save/reload, widescreen, achievements, and performance on real Switch hardware.

The integration is experimental: the first build may reveal incompatible platform API calls, source guards, or linking requirements. Successful compilation alone does not prove a playable game. Do not install a generated NRO over the original EarthBound file or point it at vanilla saves. Never label the original earthbound-switch build as the Companion build.

## Licensing

See the upstream UPSTREAM.md, LEGAL.md and LICENSE.md. Companion's original code has a permissive license, but its underlying C engine has unresolved distribution permission per upstream notes, and some Redux components are GPL. Do not publish ROMs, game assets, or a combined binary unless the applicable permissions are resolved.

## Proposed SD layout (not implemented)

    switch/earthbound-companion/EarthBoundCompanion.nro
    switch/earthbound-companion/assets.pak
    switch/earthbound-companion/saves/

Companion's repository is not modified by this branch.


## Switch linking and runtime asset requirements

The experimental build now links a Switch compatibility module for Companion's
optional desktop MSU1 audio, diagnostic video functions, and savestate scratch.
These hooks **do not** implement desktop-only MSU1 PCM playback, post-processing,
or diagnostic screenshots; standard SPC700 audio and 400x224 PPU rendering are
the intended first port targets. Do not count optional features as verified.

To boot, the Switch requires a **compatible Companion runtime asset pack** at
`sdmc:/switch/earthbound-companion/assets.pak`, produced by the upstream
Companion/ebtools asset pipeline from the user's own legally obtained EarthBound
ROM. A vanilla project assets.pak or extracted ROM files are not interchangeable:
the runtime verifies pack format, layout hash and asset count before starting.
The NRO does not bundle the proprietary game assets. If the pack is missing or
incompatible, startup stops and writes an error code to
`sdmc:/switch/earthbound-companion/startup.log` rather than running on invalid data.

A completed cross-compile does not demonstrate launch or gameplay. Validate
assets, input, audio, saves and Companion features on Switch before treating the
port as playable. Keep the experimental NRO separate from vanilla EarthBound.
