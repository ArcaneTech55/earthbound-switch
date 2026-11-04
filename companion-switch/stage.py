#!/usr/bin/env python3
"""Stage the actual patched Companion C core with the proven vanilla Switch SDL backend.

This script *does not* claim source compatibility or a successful NRO.
A separate destination protects the vanilla Switch port and the pinned Companion sources.
"""
from pathlib import Path
import argparse
import shutil

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "companion-switch" / ".work"
NATIVE = WORK / "native"
STAGE = WORK / "switch-stage"
PORT = ROOT / "port" / "switch"
COMPANION = WORK / "companion"

def replace_required(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise RuntimeError(f"Unexpected {label}: missing {old!r}")
    return text.replace(old, new)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--reset-stage", action="store_true", help="Remove only the generated switch-stage checkout")
    args = ap.parse_args()
    if not (NATIVE / "src" / "CMakeLists.txt").is_file():
        ap.error("Missing patched Companion source: run bash companion-switch/prepare.sh")
    if not (COMPANION / "Achievements" / "Core" / "src" / "companion_achievements.c").is_file():
        ap.error("Missing Companion achievements core: verify companion-switch/.work/companion")
    if not (PORT / "CMakeLists.txt").is_file():
        ap.error("Missing known Switch backend")
    if STAGE.exists():
        if not args.reset_stage:
            ap.error("Stage exists: use --reset-stage to regenerate it, keeping native/ untouched")
        shutil.rmtree(STAGE)
    shutil.copytree(NATIVE, STAGE, ignore=shutil.ignore_patterns(".git", "build", "__pycache__", ".DS_Store"),
                    symlinks=True)
    shutil.copytree(COMPANION / "Achievements", STAGE / "Achievements")

    # Companion's diagnostic self-tests use fprintf(stderr, ...); include the
    # standard declaration only in the isolated generated copy. Do not alter
    # the upstream source checkout or remove the self-tests.
    for name in ("battle.c", "battle_ui.c"):
        source_file = STAGE / "src" / "game" / name
        source_text = source_file.read_text()
        if "#include <stdio.h>" not in source_text:
            source_file.write_text("#include <stdio.h>\n" + source_text)

    dst = STAGE / "port" / "switch"
    shutil.copytree(PORT, dst, ignore=shutil.ignore_patterns("build", ".DS_Store"), dirs_exist_ok=True)

    # Companion's engine needs dynamic, versioned asset packs. Do NOT embed
    # the vanilla binary tables from the already-compiled EarthBound port.
    cmake = dst / "CMakeLists.txt"
    s = cmake.read_text()
    s = replace_required(s, "set(EB_ENABLE_AUDIO ON CACHE BOOL",
                         'set(EB_RUNTIME_ASSETS ON CACHE BOOL "Companion runtime asset packs" FORCE)\nset(EB_ENABLE_AUDIO ON CACHE BOOL',
                         "Switch CMake setup")
    # The upstream achievements patch expects ../../Achievements/Core relative to
    # native/src. In our isolated stage it lives alongside src, so override
    # the path explicitly (rather than touching the read-only native checkout).
    s = s.replace("EarthboundSwitch", "EarthBoundCompanion")
    s = replace_required(s, 'add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../src"',
                         'set(EB_ACHIEVEMENTS_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Achievements/Core" CACHE PATH "Companion achievement core" FORCE)\nadd_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../src"',
                         "Companion achievement core path")
    s = s.replace('NAME "EarthBound Switch"', 'NAME "EarthBound Companion"')
    s = s.replace('AUTHOR "EarthBound Switch Homebrew"', 'AUTHOR "Companion Switch community port"')
    cmake.write_text(s)

    makefile = dst / "Makefile"
    s = makefile.read_text().replace("EarthboundSwitch.nro", "EarthBoundCompanion.nro")
    # Runtime pack mode does not require the legacy embedded asset manifest.
    s = "\n".join(line for line in s.split("\n")
                  if 'test -f "../../asm/bin/assets.manifest"' not in line)
    makefile.write_text(s)

    main_file = dst / "main.c"
    s = main_file.read_text()
    s = replace_required(s, '"/switch/earthbound-switch"', '"/switch/earthbound-companion"', "Switch main")
    s = s.replace("EarthBound Switch boot diagnostics", "EarthBound Companion Switch boot diagnostics")
    s = s.replace('"/switch/earthbound-switch/', '"/switch/earthbound-companion/')
    main_file.write_text(s)

    for name in ("platform/switch_sdl2_video.c", "platform/switch_sdl2_input.c"):
        file = dst / name
        file.write_text(file.read_text().replace("/switch/earthbound-switch/", "/switch/earthbound-companion/"))

    print(f"Prepared isolated Companion Switch stage: {STAGE}")
    print("Source: prepared Companion C engine + existing libnx/SDL2 Switch frontend")
    print("Original native source and working vanilla EarthBound checkout unchanged.")
    print("Next: bash companion-switch/build.sh")

if __name__ == "__main__":
    main()
