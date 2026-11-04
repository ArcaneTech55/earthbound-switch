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

    # Some Companion C modules use stdio for diagnostics or formatting without
    # including its declarations. GCC 16 treats these as hard errors. Scan all
    # staged game C files in one pass instead of fixing one failure per build.
    # Do not touch the pinned source checkout; avoid generated/vendor sources.
    import re
    stdio_calls = re.compile(r"\b(?:fprintf|printf|sprintf|snprintf|vfprintf|"
                             r"vsnprintf|fopen|fclose|fread|fwrite|fflush|"
                             r"fseek|ftell|perror|stderr|stdout|stdin)\b")
    fixed = []
    for source_file in (STAGE / "src").rglob("*.c"):
        if "vendor" in source_file.relative_to(STAGE / "src").parts:
            continue
        source_text = source_file.read_text()
        if (stdio_calls.search(source_text)
                and "#include <stdio.h>" not in source_text
                and '#include "stdio.h"' not in source_text):
            source_file.write_text("#include <stdio.h>\n" + source_text)
            fixed.append(source_file.relative_to(STAGE).as_posix())
    print(f"Staged {len(fixed)} missing stdio includes:", ", ".join(fixed))

    # libnx does not have POSIX mmap. Use a bounded malloc/read/free backend
    # specifically for Switch, preserving the existing validated pak parser.
    runtime = STAGE / "src" / "data" / "runtime_assets.c"
    runtime_c = runtime.read_text()
    runtime_c = replace_required(
        runtime_c, "#ifdef _WIN32\n#include <windows.h>",
        "#if defined(__SWITCH__)\n/* Read assets.pak into owned heap memory on libnx. */\n#elif defined(_WIN32)\n#include <windows.h>",
        "runtime asset platform includes"
    )
    runtime_c = replace_required(
        runtime_c,
        '    char fallback[PATH_MAX];',
        '    char fallback[PATH_MAX];',
        "default asset path"  # guarded below with Switch-specific path
    )
    runtime_c = replace_required(
        runtime_c,
        '#if defined(_WIN32)\n    const char *appdata',
        '#if defined(__SWITCH__)\n    int n = snprintf(buf, buf_size, "/switch/earthbound-companion/assets.pak");\n    return n > 0 && (size_t)n < buf_size;\n#elif defined(_WIN32)\n    const char *appdata',
        "default asset pack location"
    )
    runtime_c = replace_required(
        runtime_c, 'static void *map_pak_file(const char *path, size_t *out_size) {\n#ifdef _WIN32',
        '''static void *map_pak_file(const char *path, size_t *out_size) {
#ifdef __SWITCH__
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
    long length = ftell(file);
    if (length <= 0 || length > 256L * 1024L * 1024L ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    void *data = malloc((size_t)length);
    if (!data) { fclose(file); return NULL; }
    size_t got = fread(data, 1, (size_t)length, file);
    int close_rc = fclose(file);
    if (got != (size_t)length || close_rc != 0) {
        free(data);
        return NULL;
    }
    *out_size = (size_t)length;
    return data;
#elif defined(_WIN32)''',
        "runtime pack mapping backend"
    )
    runtime_c = replace_required(
        runtime_c, 'static void unmap_pak_file(void *ptr, size_t size) {\n#ifdef _WIN32',
        '''static void unmap_pak_file(void *ptr, size_t size) {
#ifdef __SWITCH__
    (void)size;
    free(ptr);
#elif defined(_WIN32)''',
        "runtime pack unmapping backend"
    )
    runtime.write_text(runtime_c)

    # The achievement adapter was wired only by desktop main.c. Its
    # POSIX directory builder rejects the libnx "sdmc:" device prefix.
    # Build an SD-friendly path and skip desktop flock() on single-app Switch.
    achievements = STAGE / "src" / "game" / "companion_achievements.c"
    achievements_code = achievements.read_text()
    achievements_code = replace_required(
        achievements_code,
        "if (!(i==2 && p[1]==':') && p[i-1]!='/' && p[i-1]!='\\\\' && !directory_one(p))",
        "if (!(i==2 && p[1]==':') && !(i==5 && strncmp(p, \"sdmc:\", 5)==0) && p[i-1]!='/' && p[i-1]!='\\\\' && !directory_one(p))",
        "Switch achievement SD device root")
    # libnx doesn't expose desktop flock headers.
    achievements_code = replace_required(
        achievements_code, "#include <sys/file.h>",
        "#ifndef __SWITCH__\n#include <sys/file.h>\n#endif",
        "Switch achievement locking headers")
    achievements_code = replace_required(
        achievements_code, "if(flock(lock_handle,LOCK_EX|LOCK_NB)!=0){close_store();return false;}",
        "#ifndef __SWITCH__\n if(flock(lock_handle,LOCK_EX|LOCK_NB)!=0){close_store();return false;}\n#endif",
        "Switch achievement profile lock")
    # If SD rename cannot overwrite, keep the current profile in a backup.
    achievements_code = replace_required(
        achievements_code,
        "ok=rename(temp,profile)==0;",
        """if(rename(temp,profile)==0) {
   ok=true;
  } else {
   char backup[4102]; snprintf(backup,sizeof(backup),"%s.bak",profile);
   remove(backup);
   if(rename(profile,backup)==0) {
    ok=rename(temp,profile)==0;
    if(!ok)rename(backup,profile);
   } else ok=false;
  }""",
        "Achievement profile safe replacement")
    achievements.write_text(achievements_code)

    dst = STAGE / "port" / "switch"
    shutil.copytree(PORT, dst, ignore=shutil.ignore_patterns("build", ".DS_Store"), dirs_exist_ok=True)

    # The reused Unix audio implementation compiles in the Switch frontend.
    # GCC 16 requires the stdio diagnostics declarations as well.
    audio_source = STAGE / "port" / "unix" / "platform" / "sdl2_audio.c"
    audio_text = audio_source.read_text()
    if "#include <stdio.h>" not in audio_text:
        audio_source.write_text("#include <stdio.h>\n" + audio_text)

    # Link the Companion-only platform extension functions missing from the
    # vanilla Switch frontend, without modifying the upstream engine sources.
    shutil.copy2(ROOT / "companion-switch" / "platform_compat.c",
                 dst / "platform" / "companion_compat.c")

    shutil.copy2(ROOT / "companion-switch" / "settings_backend.c",
                 dst / "platform" / "companion_settings.c")

    shutil.copy2(ROOT / "companion-switch" / "switch_save.c",
                 dst / "platform" / "companion_save.c")

    shutil.copy2(ROOT / "companion-switch" / "msu_audio_switch.c",
                 dst / "platform" / "companion_msu_audio.c")

    # Companion's engine needs dynamic, versioned asset packs. Do NOT embed
    # the vanilla binary tables from the already-compiled EarthBound port.
    cmake = dst / "CMakeLists.txt"
    s = cmake.read_text()
    s = replace_required(s, "    platform/switch_debug.c",
                         "    platform/switch_debug.c\n    platform/companion_compat.c",
                         "Companion compatibility link source")
    # Companion's desktop audio source is compiled into the frontend, not
    # earthbound_game. Without this definition it silently compiles NO-OP stubs.
    s = replace_required(
        s, 'target_compile_definitions(earthbound_switch PRIVATE __SWITCH__=1)',
        'target_compile_definitions(earthbound_switch PRIVATE __SWITCH__=1 EB_ENABLE_AUDIO=1)\n'
        'target_compile_definitions(earthbound_game PRIVATE EB_HAVE_SETTINGS_BACKEND=1)',
        "Switch audio and settings compile flags")
    s = replace_required(
        s, '    platform/companion_compat.c',
        '    platform/companion_compat.c\n    platform/companion_settings.c',
        "Companion settings backend source")
    s = replace_required(s, "    platform/companion_settings.c",
                         "    platform/companion_settings.c\n    platform/companion_msu_audio.c",
                         "MSU Switch PCM source")
    s = replace_required(
        s, "    ../unix/platform/sdl2_save.c",
        "    platform/companion_save.c",
        "Switch SRAM save backend")
    s = replace_required(s, "set(EB_ENABLE_AUDIO ON CACHE BOOL",
                         'set(EB_RUNTIME_ASSETS ON CACHE BOOL "Companion runtime asset packs" FORCE)\nset(EB_ENABLE_AUDIO ON CACHE BOOL',
                         "Switch CMake setup")
    # Only game core knows that libnx is the target for SD profile storage.
    s = replace_required(
        s, 'target_compile_definitions(earthbound_game PRIVATE EB_HAVE_SETTINGS_BACKEND=1)',
        'target_compile_definitions(earthbound_game PRIVATE EB_HAVE_SETTINGS_BACKEND=1 __SWITCH__=1)',
        "Switch engine platform definition")
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
    # Companion is a runtime-pak engine. Initialize the verified asset table
    # before game_init(); the old vanilla frontend never needed this step.
    # Settings were never loaded by the standalone Switch entrypoint.
    s = replace_required(s, '#include "game_main.h"',
                         '#include "game_main.h"\n#include "platform/pc_options.h"',
                         "Companion options header")
    s = replace_required(s, '    trace("Initializing SDL subsystems");',
                         '    pc_options_defaults();\n    trace("Companion audio volume initialized");\n    trace("Initializing SDL subsystems");',
                         "Companion default PC options")
    s = replace_required(s, '#include "platform/platform.h"',
                         '#include "platform/platform.h"\n#include "game/settings.h"',
                         "Companion settings header")
    s = replace_required(s, '    trace("Initializing timer");',
                         '    settings_load();\n    trace("Companion settings loaded");\n    trace("Initializing timer");',
                         "Companion settings initialization")
    s = replace_required(s, '#include "game_main.h"',
                         '#include "game_main.h"\n#include "data/runtime_assets.h"',
                         "Companion asset loader header")
    s = replace_required(s, '    trace("Initializing SDL subsystems");',
                         '''    trace("Loading Companion runtime assets");
    EbAssetLoadResult pak_status = eb_runtime_assets_load(
        "/switch/earthbound-companion/assets.pak");
    if (pak_status != EB_ASSETS_OK) {
        trace("ERROR assets.pak load status: %d", (int)pak_status);
        fprintf(stderr, "Companion assets.pak missing or invalid: %d\\n",
                (int)pak_status);
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    trace("Companion assets.pak validated and loaded");

    trace("Initializing SDL subsystems");''',
                         "Companion pre-game asset load")
    s = replace_required(s, '    if (sdl_started) SDL_Quit();',
                         '    if (sdl_started) SDL_Quit();\n    if (eb_runtime_assets_ready()) eb_runtime_assets_unload();',
                         "Companion asset shutdown")
    # libnx paths must be device-qualified; /switch/... may refer to the
    # wrong devoptab root depending on how hbmenu launched the NRO.
    s = s.replace('"/switch/earthbound-companion', '"sdmc:/switch/earthbound-companion')
    s = s.replace('"/switch"', '"sdmc:/switch"')
    s = replace_required(s, '    int exit_code = EXIT_SUCCESS;',
                         '    int exit_code = EXIT_SUCCESS;\n    const char *startup_error = NULL;',
                         "boot error state")
    s = replace_required(s,
                         '        return EXIT_FAILURE;\n    }\n    sd_mounted = true;',
                         '''        startup_error = "Cannot mount the SD card";
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    sd_mounted = true;''',
                         "early SD mount failure")
    s = replace_required(s, '        perror("Cannot select sdmc filesystem");',
                         '''        perror("Cannot select sdmc filesystem");
        startup_error = "Cannot select SD filesystem";''',
                         "SD working directory failure")
    s = replace_required(s, '    startup_log = fopen("sdmc:/switch/earthbound-companion/startup.log", "w");',
                         '''    startup_log = fopen("sdmc:/switch/earthbound-companion/startup.log", "w");
    if (!startup_log)
        fprintf(stderr, "Cannot create Companion startup.log (errno=%d)\\n", errno);''',
                         "SD diagnostics")
    s = replace_required(s, '        trace("ERROR assets.pak load status: %d", (int)pak_status);',
                         '''        trace("ERROR assets.pak load status: %d", (int)pak_status);
        startup_error = "Companion assets.pak missing or incompatible";''',
                         "assets startup error")
    for marker, message in (
        ('trace("ERROR SDL_Init: %s", SDL_GetError());', 'SDL initialization failed'),
        ('trace("ERROR platform_video_init: %s", SDL_GetError());', 'Switch video initialization failed'),
        ('trace("ERROR input initialization");', 'Controller initialization failed'),
        ('trace("ERROR save initialization");', 'Save initialization failed'),
        ('trace("ERROR timer initialization");', 'Timer initialization failed'),
        ('trace("ERROR audio initialization: %s", SDL_GetError());', 'Audio initialization failed'),
    ):
        s = replace_required(s, marker, marker + '\n        startup_error = "' + message + '";',
                             "startup failure: " + message)
    s = replace_required(s, '    if (startup_log) { fclose(startup_log); startup_log = NULL; }',
                         '''    if (startup_log) { fclose(startup_log); startup_log = NULL; }
    if (exit_code != EXIT_SUCCESS) {
        /* Console feedback is a fallback when hbmenu suppresses stderr.
         * SDL is already shut down so we don't compete for the display. */
        consoleInit(NULL);
        printf("EarthBound Companion Switch\\n\\n");
        printf("STARTUP FAILED\\n");
        printf("%s\\n", startup_error ? startup_error : "Unknown boot error");
        printf("\\nCheck /switch/earthbound-companion/startup.log\\n");
        printf("Make sure assets.pak is in the same SD folder.\\n");
        printf("\\nReturning to Homebrew in 12 seconds.\\n");
        consoleUpdate(NULL);
        for (int i = 0; i < 120 && appletMainLoop(); ++i)
            svcSleepThread(100000000ULL);
        consoleExit(NULL);
    }''',
                         "screen error presentation")
    # Save initialization now probes for actual SD write access; give a
    # meaningful diagnostic before running any gameplay.
    # Activate achievements for real gameplay; the desktop main used to
    # do this, but the standalone Switch main never did.
    s = replace_required(s, '#include "game/settings.h"',
                         '#include "game/settings.h"\n#include "game/eb_achievements.h"',
                         "Switch achievements header")
    s = replace_required(s, '    trace("game_init returned");',
                         '''    trace("game_init returned");
    if (eb_achievements_init(
            "sdmc:/switch/earthbound-companion/achievements", false)) {
        trace("Achievements initialized and profile is writable");
    } else {
        trace("WARNING achievement profile initialization failed");
    }''',
                         "Switch achievement initialization")
    # Autodetect an MSU-1 music pack on the SD card before game starts.
    s = replace_required(s, '#include <string.h>',
                         '''#include <string.h>
bool platform_audio_msu_autodetect_name(const char *dir, char *out, size_t size);
void platform_audio_msu_load(const char *dir, const char *name);
void platform_audio_msu_unload(void);''',
                         "MSU Switch frontend declarations")
    s = replace_required(s, '    trace("Entering game_init");',
                         '''    char msu_pack_name[128] = {0};
    const char *msu_dir = "sdmc:/switch/earthbound-companion/msu";
    if (platform_audio_msu_autodetect_name(msu_dir, msu_pack_name,
                                            sizeof(msu_pack_name))) {
        platform_audio_msu_load(msu_dir, msu_pack_name);
        trace("MSU-1 pack detected: %s", msu_pack_name);
    } else {
        trace("MSU-1 pack missing; standard SPC music remains active");
    }
    trace("Entering game_init");''',
                         "MSU startup autodetection")
    s = replace_required(s, '    if (audio_started) platform_audio_shutdown();',
                         '    if (audio_started) platform_audio_shutdown();\n    platform_audio_msu_unload();',
                         "MSU shutdown")
    main_file.write_text(s)

    for name in ("platform/switch_sdl2_video.c", "platform/switch_sdl2_input.c"):
        file = dst / name
        file.write_text(file.read_text().replace("/switch/earthbound-switch/", "/switch/earthbound-companion/"))

    # Implement basic Companion Config effects using the Switch SDL renderer.
    video = dst / "platform" / "switch_sdl2_video.c"
    video_text = video.read_text()
    video_text = replace_required(
        video_text, '#include "platform/platform.h"',
        '#include "platform/platform.h"\n#include "game/settings.h"',
        "Switch video settings header")
    video_text = replace_required(
        video_text, '    eb_switch_scene_art_frame(frame,EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT);',
        '''    eb_switch_scene_art_frame(frame,EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT);
    if (engine_fx_scanlines) {
        for (int y = 1; y < EB_VIEWPORT_HEIGHT; y += 2) {
            for (int x = 0; x < EB_VIEWPORT_WIDTH; ++x) {
                pixel_t *px = &frame[(size_t)y * EB_VIEWPORT_WIDTH + x];
                unsigned rgb = pixel_to_rgb888(*px);
                *px = PIXEL_RGB(((rgb >> 16) & 255u) * 3u / 4u,
                                ((rgb >> 8) & 255u) * 3u / 4u,
                                (rgb & 255u) * 3u / 4u);
            }
        }
    }
    SDL_SetTextureScaleMode(frame_texture,
        engine_fx_antialiasing ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);''',
        "Switch Config scanlines and filter")
    video_text = replace_required(
        video_text, '    SDL_RenderCopy(renderer,frame_texture,NULL,&dst);',
        '''    SDL_Rect src_rect = {0, 0, EB_VIEWPORT_WIDTH, EB_VIEWPORT_HEIGHT};
    if (engine_aspect_ratio == ASPECT_RATIO_4_3 &&
        EB_VIEWPORT_WIDTH > SNES_WIDTH) {
        src_rect.x = (EB_VIEWPORT_WIDTH - SNES_WIDTH) / 2;
        src_rect.w = SNES_WIDTH;
        EbWsRect fitted = eb_scale_destination(
            (EbWsSize){src_rect.w,src_rect.h},
            (EbWsSize){width,height},scale_mode);
        dst = (SDL_Rect){fitted.x,fitted.y,fitted.width,fitted.height};
    }
    SDL_RenderCopy(renderer,frame_texture,&src_rect,&dst);''',
        "Switch Config 4:3 aspect ratio")
    video.write_text(video_text)

    # In-game Alternate Controls must change Nintendo face-button mappings.
    input_file = dst / "platform" / "switch_sdl2_input.c"
    input_text = input_file.read_text()
    input_text = replace_required(
        input_text, '#include "game/game_state.h"',
        '#include "game/game_state.h"\n#include "game/settings.h"',
        "Switch alternate controls header")
    input_text = replace_required(
        input_text, '    uint16_t next = eb_switch_map_pad(buttons);',
        '''    if (engine_alt_controls) {
        uint32_t face = 0;
        if (buttons & EB_SW_BTN_A) face |= EB_SW_BTN_B;
        if (buttons & EB_SW_BTN_B) face |= EB_SW_BTN_A;
        if (buttons & EB_SW_BTN_X) face |= EB_SW_BTN_Y;
        if (buttons & EB_SW_BTN_Y) face |= EB_SW_BTN_X;
        buttons = (buttons & ~(EB_SW_BTN_A | EB_SW_BTN_B |
                                EB_SW_BTN_X | EB_SW_BTN_Y)) | face;
    }
    uint16_t next = eb_switch_map_pad(buttons);''',
        "Switch alternate button mapping")
    input_file.write_text(input_text)

    # Wire the vanilla settings overlay's scale callback to the actual
    # Switch presentation state, instead of leaving a dangling symbol.
    video = dst / "platform" / "switch_sdl2_video.c"
    video_code = video.read_text()
    video_code += """
void eb_switch_video_set_scale(int mode) {
    if (mode < EB_SCALE_ASPECT_NEAREST || mode > EB_SCALE_ASPECT_LINEAR)
        return;
    scale_mode = (EbScaleMode)mode;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                mode == EB_SCALE_ASPECT_LINEAR ? "linear" : "nearest");
    if (frame_texture)
        SDL_SetTextureScaleMode(frame_texture,
            mode == EB_SCALE_ASPECT_LINEAR ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
}
"""
    video.write_text(video_code)

    print(f"Prepared isolated Companion Switch stage: {STAGE}")
    print("Source: prepared Companion C engine + existing libnx/SDL2 Switch frontend")
    print("Original native source and working vanilla EarthBound checkout unchanged.")
    print("Next: bash companion-switch/build.sh")

if __name__ == "__main__":
    main()
