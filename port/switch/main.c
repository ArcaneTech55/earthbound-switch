/*
 * EarthBound Switch application entry point.
 *
 * The SDL2 video/input/audio backends live in port/switch/platform/ (or reuse
 * the Unix SDL2 backend), implementing src/platform/platform.h. This file
 * owns the libnx applet lifecycle, persistent SD mount and engine loop.
 */
#include <switch.h>
#include <SDL.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#include "game_main.h"
#include "platform/platform.h"

/* Path selection provided by port/unix/platform/sdl2_save.c. */
extern void platform_save_set_path(const char *path);

/* Write startup diagnostics to SD because hbmenu usually hides stderr. */
static FILE *startup_log;

static void trace(const char *format, ...) {
    if (!startup_log) return;
    va_list args;
    va_start(args, format);
    vfprintf(startup_log, format, args);
    va_end(args);
    fputc('\\n', startup_log);
    fflush(startup_log);
}

static bool ensure_directory(const char *path) {
    if (mkdir(path, 0777) == 0) return true;
    return errno == EEXIST;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int exit_code = EXIT_SUCCESS;
    bool sd_mounted = false;
    bool sdl_started = false;
    bool video_started = false;
    bool input_started = false;
    bool timer_started = false;
    bool audio_started = false;

    /* SD must be mounted before game_init: SRAM integrity checks can write. */
    /* Homebrew launcher may have mounted sdmc already. */
    const Result mount = fsdevMountSdmc();
    if (R_FAILED(mount) && chdir("sdmc:/") != 0) {
        fprintf(stderr, "Cannot mount SD card (0x%08lX)\n",
                (unsigned long)mount);
        return EXIT_FAILURE;
    }
    sd_mounted = true;

    /* Explicitly make sdmc the current/default devoptab device. This makes
     * absolute /switch/... paths refer to the SD card, independent of the
     * application's startup cwd. libnx also supports sdmc:/... paths. */
    if (chdir("sdmc:/") != 0) {
        perror("Cannot select sdmc filesystem");
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }

    if (!ensure_directory("/switch") ||
        !ensure_directory("/switch/earthbound-switch")) {
        fprintf(stderr, "Cannot create EarthBound save directory\n");
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    startup_log = fopen("/switch/earthbound-switch/startup.log", "w");
    trace("EarthBound Switch boot diagnostics");
    trace("SD mounted; working directory set; save directory exists");
    platform_save_set_path("/switch/earthbound-switch/earthbound.srm");

    /* The Switch SDL port selects its window renderer, audio device and
     * controller backend. The video module creates the accelerated window
     * and uploads the software PPU framebuffer every rendered frame. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    const Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER |
                         SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;
    trace("Initializing SDL subsystems");
    if (SDL_Init(flags) != 0) {
        trace("ERROR SDL_Init: %s", SDL_GetError());
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    sdl_started = true;
    trace("SDL initialized");

    trace("Initializing video");
    if (!platform_video_init()) {
        trace("ERROR platform_video_init: %s", SDL_GetError());
        fprintf(stderr, "platform_video_init: %s\n", SDL_GetError());
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    video_started = true;
    trace("Video initialized");

    trace("Initializing input");
    if (!platform_input_init()) {
        trace("ERROR input initialization");
        fprintf(stderr, "platform_input_init failed\n");
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    input_started = true;
    trace("Input initialized");

    trace("Initializing save system");
    if (!platform_save_init()) {
        trace("ERROR save initialization");
        fprintf(stderr, "platform_save_init failed\n");
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }

    trace("Initializing timer");
    if (!platform_timer_init()) {
        trace("ERROR timer initialization");
        fprintf(stderr, "platform_timer_init failed\n");
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    timer_started = true;
    trace("Timer initialized");

    /* Initialize game AFTER SRAM and BEFORE audio (as the Unix port does). */
    trace("Entering game_init");
    game_init();
    trace("game_init returned");

    trace("Initializing audio");
    if (!platform_audio_init()) {
        trace("ERROR audio initialization: %s", SDL_GetError());
        fprintf(stderr, "platform_audio_init: %s\n", SDL_GetError());
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    audio_started = true;
    trace("Audio initialized; beginning game loop");
    platform_timer_frame_start();

    /* The core owns frame pacing/input polling/render dispatch inside
     * host_process_frame(). Its PPU scanlines go through platform_video_*
     * into the SDL texture. Calling host_root_boundary() at the outermost
     * loop level also preserves savestate synchronization. */
    unsigned long frame_count = 0;
    while (appletMainLoop() && !platform_input_quit_requested()) {
        if (frame_count == 0) trace("First game loop frame");
        if (++frame_count == 60) trace("60 game loop frames completed");
        game_loop_step();
        host_process_frame();
        host_root_boundary();
    }

shutdown:
    trace("Shutting down (exit_code=%d)", exit_code);
    if (audio_started) platform_audio_shutdown();
    if (timer_started) platform_timer_shutdown();
    if (input_started) platform_input_shutdown();
    if (video_started) platform_video_shutdown();
    if (sdl_started) SDL_Quit();
    if (startup_log) { fclose(startup_log); startup_log = NULL; }
    if (sd_mounted) fsdevUnmountAll();
    return exit_code;
}
