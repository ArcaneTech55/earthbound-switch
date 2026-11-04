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

The Switch frontend includes native MSU-1 PCM music playback alongside standard
SPC700 effects and music. It autodetects compatible PCM packs on the SD card;
no enhanced recordings are distributed with this repository. Desktop-only
post-processing and diagnostic screenshots are still incomplete. Audio, runtime
memory use and all track transitions require actual Switch hardware testing.

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


## Optional HQ Audio (MSU-1) on Nintendo Switch

This branch now contains a Switch-specific native MSU-1 player in
`companion-switch/msu_audio_switch.c`. It reads the community-standard
`MSU1` PCM header and signed 16-bit **stereo, 44.1 kHz** samples, resamples
into SDL's audio callback, respects Companion's HQ Audio and music-volume
settings, and retains the original SPC700 sound effects. If a PCM track is not
present or is invalid, EarthBound uses its original SPC music instead.

To use it, create the following directory on the SD card:

```text
switch/earthbound-companion/msu/
  your-pack-name-1.pcm
  your-pack-name-2.pcm
  your-pack-name-3.pcm
  ...
```

File names must be `<pack-name>-<decimal-track-number>.pcm`. The game
scans the folder at launch and automatically selects the prefix from the first
matching PCM filename. Use **one pack** per folder so the prefix is not
ambiguous. The files must be real MSU1-formatted PCM, not WAV or MP3 files
renamed to PCM. Enable **HQ Audio** in EarthBound's Config menu. A new track
must be triggered for a setting change to take effect; return to title and
reload if needed. If there is no compatible pack the original music is used.

For memory safety each track is limited to 64 MiB; larger/unreadable tracks
fall back to the original music. Only one current track is buffered, and its
memory is freed when the song changes. This implementation has **not yet been
verified on a real Switch** for extended playback, track coverage, timing or
sleep/resume. Keep a backup of saves before testing. No music files are
provided or generated by the build script.
