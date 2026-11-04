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

    # The pinned native serializer has no s_known cache: its channel reads
    # go straight to the selected backend, so no cache invalidation is needed.

    dst = STAGE / "port" / "switch"
    shutil.copytree(PORT, dst, ignore=shutil.ignore_patterns("build", ".DS_Store"), dirs_exist_ok=True)

    # The reused Unix audio implementation compiles in the Switch frontend.
    # GCC 16 requires the stdio diagnostics declarations as well.
    audio_source = STAGE / "port" / "unix" / "platform" / "sdl2_audio.c"
    audio_text = audio_source.read_text()
    if "#include <stdio.h>" not in audio_text:
        audio_text="#include <stdio.h>\n" + audio_text
    audio_marker="void platform_audio_shutdown(void) {"
    if audio_marker not in audio_text:
        raise RuntimeError("Companion audio shutdown marker missing")
    audio_text=audio_text.replace(audio_marker,
        """void companion_audio_pause(bool paused) {
    if(audio_device) SDL_PauseAudioDevice(audio_device,paused?1:0);
}
void platform_audio_shutdown(void) {""",1)
    audio_source.write_text(audio_text)

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
    shutil.copy2(ROOT / "companion-switch" / "companion_hub.c",
                 dst / "platform" / "companion_hub.c")
    shutil.copy2(ROOT / "companion-switch" / "switch_states.c",
                 dst / "platform" / "companion_states.c")
    shutil.copy2(ROOT / "companion-switch" / "suspend_switch.c",
                 dst / "platform" / "companion_suspend.c")

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
    s = replace_required(s, "    platform/companion_msu_audio.c",
                         "    platform/companion_msu_audio.c\n    platform/companion_hub.c",
                         "Companion Hub SDL source")
    s = replace_required(s, "    platform/companion_hub.c",
                         "    platform/companion_hub.c\n    platform/companion_states.c\n    platform/companion_suspend.c",
                         "Companion snapshot backend")
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
    s = replace_required(
        s, 'target_compile_definitions(earthbound_game PRIVATE EB_HAVE_SETTINGS_BACKEND=1 __SWITCH__=1)',
        'target_compile_definitions(earthbound_game PRIVATE EB_HAVE_SETTINGS_BACKEND=1 EB_HAVE_SAVESTATE_BACKEND=1 __SWITCH__=1)',
        "Switch engine save backend enable")
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
    # Suspend overlays are serviced at the outermost root loop only.
    # Rewind and manual points never read/write the game's normal SRAM.
    s = replace_required(s, '#include "game/settings.h"',
                         '''#include "game/settings.h"
extern bool eb_switch_menu_open;
extern int companion_hub_action;
extern int companion_hub_slot;
extern char companion_hub_status[48];
void companion_hub_present_paused(void);
void companion_hub_invalidate_previews(void);
void companion_suspend_init(void);
void companion_suspend_frame_tick(void);
bool companion_suspend_action(int,int,const uint16_t*,int,int);''',
                         "Companion suspend frontend declarations")
    s = replace_required(s, '    trace("game_init returned");',
                         '''    trace("game_init returned");
    companion_suspend_init();''',
                         "Companion suspend storage startup")
    s = replace_required(s, '        game_loop_step();',
                         '''        if (eb_switch_menu_open) {
            /* Keep the snapshot at a root-loop boundary, not inside game logic.
             * Controller continues polling; simulation does not advance. */
            platform_input_poll();
            if (companion_hub_action) {
                int request = companion_hub_action;
                companion_hub_action = 0;
                bool ok = companion_suspend_action(request, companion_hub_slot,
                    (const uint16_t*)platform_video_get_framebuffer(),
                    EB_VIEWPORT_WIDTH, EB_VIEWPORT_HEIGHT);
                snprintf(companion_hub_status,sizeof(companion_hub_status),
                         "%s", ok ? "SUCCESS" : "UNAVAILABLE");
                if (ok) {
                    companion_hub_invalidate_previews();
                    if (request != 1) eb_switch_menu_open = false;
                }
            }
            companion_hub_present_paused();
            SDL_Delay(16);
            continue;
        }
        game_loop_step();''',
                         "Companion pause and suspend command dispatch")
    s = replace_required(s, '        host_root_boundary();',
                         '        host_root_boundary();\n        companion_suspend_frame_tick();',
                         "Companion rewind history capture")
    s = replace_required(s, '    bool audio_started = false;',
                         '''    bool audio_started = false;
    bool hub_audio_paused = false;''',
                         "Companion pause audio state")
    s = replace_required(s, 'void companion_hub_present_paused(void);',
                         '''void companion_hub_present_paused(void);
void companion_audio_pause(bool paused);''',
                         "Companion pause audio declaration")
    s = replace_required(s, '        if (eb_switch_menu_open) {',
                         '''        if (eb_switch_menu_open) {
            if (audio_started && !hub_audio_paused) {
                companion_audio_pause(true);
                hub_audio_paused = true;
            }''',
                         "Companion freeze audio when menu opens")
    s = replace_required(s, '        game_loop_step();',
                         '''        if (hub_audio_paused) {
            companion_audio_pause(false);
            hub_audio_paused = false;
        }
        game_loop_step();''',
                         "Companion resume audio when menu closes")
    main_file.write_text(s)

    for name in ("platform/switch_sdl2_video.c", "platform/switch_sdl2_input.c"):
        file = dst / name
        file.write_text(file.read_text().replace("/switch/earthbound-switch/", "/switch/earthbound-companion/"))

    # Companion visual settings: keep scanlines subtle and move filtering
    # to a 2x nearest-upscaled texture before the final bilinear pass.
    # Direct bilinear on 400x224 blurs every pixel, which looks muddy.
    video = dst / "platform" / "switch_sdl2_video.c"
    video_text = video.read_text()
    video_text = replace_required(
        video_text, '#include "platform/platform.h"',
        '#include "platform/platform.h"\n#include "game/settings.h"',
        "Switch video settings header")
    video_text = replace_required(
        video_text, 'static pixel_t frame[EB_VIEWPORT_WIDTH*EB_VIEWPORT_HEIGHT];',
        '''static pixel_t frame[EB_VIEWPORT_WIDTH*EB_VIEWPORT_HEIGHT];
/* Optional pre-scaled frame: nearest 2x followed by output bilinear gives
 * softened sub-pixel edges without full-frame low-resolution blur. */
static SDL_Texture *smooth_texture;
static pixel_t smooth_frame[EB_VIEWPORT_WIDTH*2*EB_VIEWPORT_HEIGHT*2];''',
        "Switch filter buffers")
    video_text = replace_required(
        video_text, '    return frame_texture!=NULL;',
        '''    if (!frame_texture) return false;
    smooth_texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_BGR565,
        SDL_TEXTUREACCESS_STREAMING,EB_VIEWPORT_WIDTH*2,EB_VIEWPORT_HEIGHT*2);
    if (smooth_texture)
        SDL_SetTextureScaleMode(smooth_texture,SDL_ScaleModeLinear);
    SDL_SetTextureScaleMode(frame_texture,SDL_ScaleModeNearest);
    /* The sharp renderer always works even if optional texture creation fails. */
    return true;''',
        "Switch smooth texture init")
    video_text = replace_required(
        video_text, 'void platform_video_shutdown(void) {',
        '''void platform_video_shutdown(void) {
    if(smooth_texture){SDL_DestroyTexture(smooth_texture);smooth_texture=NULL;}''',
        "Switch smooth texture cleanup")
    video_text = replace_required(
        video_text, '    eb_switch_scene_art_frame(frame,EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT);',
        '''    eb_switch_scene_art_frame(frame,EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT);
    /* 6% dimming instead of the former 25%; keep text and bright scenes
     * readable. The effect is opt-in through Companion Config. */
    if (engine_fx_scanlines) {
        for (int y = 1; y < EB_VIEWPORT_HEIGHT; y += 2) {
            for (int x = 0; x < EB_VIEWPORT_WIDTH; ++x) {
                pixel_t *px = &frame[(size_t)y * EB_VIEWPORT_WIDTH + x];
                unsigned rgb = pixel_to_rgb888(*px);
                *px = PIXEL_RGB(((rgb >> 16) & 255u) * 15u / 16u,
                                ((rgb >> 8) & 255u) * 15u / 16u,
                                (rgb & 255u) * 15u / 16u);
            }
        }
    }
    /* Build the smooth texture from exact 2x nearest neighbor pixels.
     * No blending happens in the game framebuffer or the SNES PPU. */
    bool use_smooth=engine_fx_antialiasing && smooth_texture;
    if (use_smooth) {
        const int wide=EB_VIEWPORT_WIDTH*2;
        for(int y=0;y<EB_VIEWPORT_HEIGHT;y++) {
            const pixel_t *in=frame+(size_t)y*EB_VIEWPORT_WIDTH;
            pixel_t *out0=smooth_frame+(size_t)(y*2)*wide;
            pixel_t *out1=out0+wide;
            for(int x=0;x<EB_VIEWPORT_WIDTH;x++) {
                pixel_t v=in[x];
                out0[x*2]=out0[x*2+1]=v;
                out1[x*2]=out1[x*2+1]=v;
            }
        }
        SDL_UpdateTexture(smooth_texture,NULL,smooth_frame,wide*(int)sizeof(pixel_t));
    }''',
        "Switch scanline strength and smooth pre-scale")
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
    SDL_Texture *chosen=use_smooth?smooth_texture:frame_texture;
    if(use_smooth) {
        src_rect.x*=2; src_rect.y*=2; src_rect.w*=2; src_rect.h*=2;
    }
    SDL_RenderCopy(renderer,chosen,&src_rect,&dst);''',
        "Switch Config aspect and scaled output")
    video_text = replace_required(
        video_text, '    if(SDL_UpdateTexture(frame_texture,NULL,frame,stride_bytes)!=0)return;',
        '''    if(SDL_UpdateTexture(frame_texture,NULL,frame,stride_bytes)!=0)return;''',
        "Switch existing texture upload")
    video_text = replace_required(
        video_text, '#include <SDL.h>',
        '#include <SDL.h>\nvoid companion_hub_draw(SDL_Renderer*,int,int,const uint16_t*,int,int);',
        "Companion Hub render declaration")
    video_text = replace_required(
        video_text, '    SDL_RenderPresent(renderer);',
        '    companion_hub_draw(renderer,width,height,(const uint16_t*)frame,EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT);\n    SDL_RenderPresent(renderer);',
        "Companion Hub presentation")
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
    # Replace the old invisible debug/cheat menu with useful QoL toggles.
    input_text = replace_required(
        input_text, '#include "game/settings.h"',
        '#include "game/settings.h"\n#include "game_main.h"',
        "Companion Hub input declarations")
    input_text = replace_required(
        input_text, 'static bool menu_combo_latched = false;',
        'bool eb_switch_menu_fast_forward = false;\nstatic bool fast_forward_aux_pulse = false;\nstatic bool menu_combo_latched = false;',
        "Companion Hub fast-forward state")
    input_text = replace_required(
        input_text, 'static uint32_t frame_counter = 0;',
        '''static uint32_t frame_counter = 0;
int companion_hub_action = 0;
int companion_hub_slot = 0;
char companion_hub_status[48] = {0};
static uint16_t menu_previous_pad;''',
        "Companion suspend menu state")

    old_action_start = input_text.index('static void menu_action(void) {')
    old_action_end = input_text.index('\nbool platform_headless=',old_action_start)
    input_text = input_text[:old_action_start] + '''static void menu_action(void) {
    companion_hub_status[0] = 0;
    switch (eb_switch_menu_selected) {
        case 0: eb_switch_menu_open=false; break;
        case 1: companion_hub_action=1; break;
        case 2: companion_hub_action=2; break;
        case 3: companion_hub_action=3; break;
        case 4: engine_fx_antialiasing=!engine_fx_antialiasing; break;
        case 5: engine_fx_scanlines=!engine_fx_scanlines; break;
        case 6:
            eb_switch_menu_scale=(eb_switch_menu_scale+1)%3;
            eb_switch_video_set_scale(eb_switch_menu_scale);
            break;
        case 7: eb_switch_menu_open=false; break;
    }
}
''' + input_text[old_action_end:]
    input_text = replace_required(
        input_text, '    menu_combo_latched = combo;',
        '    menu_combo_latched = combo;',
        "Menu combo latch")
    input_text = replace_required(
        input_text, '    if (eb_switch_menu_open) {\n        uint16_t pressed = eb_switch_pad_pressed(held, next);',
        '''    if (eb_switch_menu_open) {
        uint16_t pressed = eb_switch_pad_pressed(menu_previous_pad, next);
        menu_previous_pad = next;''',
        "Menu edge-triggered input")
    input_text = replace_required(
        input_text, '(eb_switch_menu_selected + 5) % 6;',
        '(eb_switch_menu_selected + 7) % 8;',
        "Menu up selection")
    input_text = replace_required(
        input_text, '(eb_switch_menu_selected + 1) % 6;',
        '(eb_switch_menu_selected + 1) % 8;',
        "Menu down selection")
    input_text = replace_required(
        input_text, '        if (pressed & PAD_A) menu_action();',
        '''        if (pressed & PAD_LEFT) companion_hub_slot=(companion_hub_slot+3)%4;
        if (pressed & PAD_RIGHT) companion_hub_slot=(companion_hub_slot+1)%4;
        if (pressed & PAD_A) menu_action();''',
        "Menu suspend point selection")
    input_text = replace_required(
        input_text, '    } else if (eb_switch_menu_auto_heal && frame_counter % 60 == 0) {',
        '''    } else if (eb_switch_menu_auto_heal && frame_counter % 60 == 0) {
        menu_previous_pad=0;''',
        "Menu key-release tracking")
    input_text = replace_required(
        input_text, 'uint16_t platform_input_get_aux(void) { return 0; }',
        '''uint16_t platform_input_get_aux(void) {
    if (fast_forward_aux_pulse) {
        fast_forward_aux_pulse = false;
        return AUX_FAST_FORWARD;
    }
    return 0;
}''',
        "Companion Hub fast-forward auxiliary button")
    input_text = replace_required(
        input_text, '    held = next;',
        '''    if (!eb_switch_menu_open) menu_previous_pad=0;
    held = next;''',
        "Companion menu button release")
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
    /* Scaling mode chooses layout, not softness. Pixel filtering is
     * controlled solely by the Companion anti-aliasing setting. */
    if (frame_texture) SDL_SetTextureScaleMode(frame_texture,SDL_ScaleModeNearest);
    if (smooth_texture) SDL_SetTextureScaleMode(smooth_texture,SDL_ScaleModeLinear);
}
"""
    video_code += """
/* Re-present the last frame while input polls but the game simulation stops.
 * No scene art, filtering, or scanline processing is applied a second time. */
void companion_hub_present_paused(void) {
    if (!renderer || !frame_texture) return;
    int width=1280,height=720;
    if(SDL_GetRendererOutputSize(renderer,&width,&height)!=0) {
        width=1280;height=720;
    }
    SDL_Rect source={0,0,EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT};
    if(engine_aspect_ratio==ASPECT_RATIO_4_3 && EB_VIEWPORT_WIDTH>SNES_WIDTH){
        source.x=(EB_VIEWPORT_WIDTH-SNES_WIDTH)/2;source.w=SNES_WIDTH;
    }
    EbWsRect fitted=eb_scale_destination(
        (EbWsSize){source.w,source.h},(EbWsSize){width,height},scale_mode);
    SDL_Rect dest={fitted.x,fitted.y,fitted.width,fitted.height};
    SDL_Texture *chosen=(engine_fx_antialiasing&&smooth_texture)?
                         smooth_texture:frame_texture;
    if(chosen==smooth_texture){
        source.x*=2;source.y*=2;source.w*=2;source.h*=2;
    }
    SDL_SetRenderDrawColor(renderer,0,0,0,255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer,chosen,&source,&dest);
    companion_hub_draw(renderer,width,height,(const uint16_t*)frame,
                       EB_VIEWPORT_WIDTH,EB_VIEWPORT_HEIGHT);
    SDL_RenderPresent(renderer);
}
"""
    video.write_text(video_code)

    print(f"Prepared isolated Companion Switch stage: {STAGE}")
    print("Source: prepared Companion C engine + existing libnx/SDL2 Switch frontend")
    print("Original native source and working vanilla EarthBound checkout unchanged.")
    print("Next: bash companion-switch/build.sh")

if __name__ == "__main__":
    main()
