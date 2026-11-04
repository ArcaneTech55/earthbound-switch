# Mother 2 Deluxe v3.0.16 native Switch port — compatibility audit

This is a feasibility audit for an **independently supplied, pre-patched ROM**. No copyrighted ROM bytes are included in this repository.

## Input checks

- Vanilla EarthBound (USA) dump: 3,145,728 bytes, CRC32 `DC9BB451`.
- Mother 2 Deluxe v3.0.16 user-supplied ROM: 6,291,456 bytes, SHA256 `635b4a148cfd1abf8cb43b37726b094183a81a92d11043090d7df1fc391a0f1a`.
- Deluxe is unheadered (the filesize is divisible by 32768).
- 2,141,658 of the original 3,145,728 bytes (68.1%) differ at corresponding offsets. The additional 3 MiB contains 2,539,423 nonzero bytes.
- Both ROMs contain an EarthBound internal SNES title at LoROM/HiROM header location `0xFFC0`; the patched ROM also has an additional header at `0x40FFC0`.

These data prove **major expansion and code/data relocation**, not a clean input for the vanilla ROM address extractor.

## Native code versus patched ROM

The existing Switch `.nro` runs a C port of the original game's logic. It **does not execute SNES machine code**, nor does it expose a SNES emulator. Therefore replacing `donor.sfc` with a modded ROM and calling `make extract` cannot preserve Deluxe's ASM gameplay features.

Mother 2 Deluxe includes (among other things) rebalanced enemies and PSI, upgraded dungeon/boss systems, new Jeff tools, extra content and custom interfaces. Version 3.0 adds substantially more systems, including difficulty options, achievements, costumes and enemy animations. Their gameplay logic requires independent C implementation and verification.

## Safe migration plan

1. **Keep vanilla stable**: maintain the known working base .nro; use a separate Deluxe development branch.
2. **Build an asset compatibility map**: locate relocated tables, compressed graphics, dialogue formats and expansion banks by comparing ROMs, instead of blindly applying original offsets.
3. **Migrate data**: add versioned ROM extractors, load Deluxe-specific tables into separate generated assets, compare game screenshots and data records.
4. **Reimplement mechanisms**: implement custom battle state/AI, new PSI/tools, dungeon progression, UI hooks, battle sprites, events and achievements in the C engine.
5. **Validate gameplay**: reproduce reference behavior in SNES bsnes/Mesen-S and native Switch, including saves and all bonus content.

Do not overwrite existing `earthbound.srm` saves with a Deluxe build until a compatibility migration has been designed and tested.

## Native settings/mod menu foundation

A controller-operated SDL overlay lives in the Switch port. Open it with **ZL + PLUS** (or press the left stick and PLUS), navigate with D-pad, choose with A and close with B.

Current options: display scaling (persisted in `scaling.cfg`), quick text speed (native typewriter delay, without increasing FPS), auto-heal, heal party, add $1,000, and return to game.

This overlay is **not Mother 2 Deluxe support**. The patched ROM has not yet been transformed into a working native game, and the overlay needs on-device compile/run tests.
