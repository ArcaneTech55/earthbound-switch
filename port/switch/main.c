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

#include "game_main.h"
#include "platform/platform.h"

/* Path selection provided by port/unix/platform/sdl2_save.c. */
extern void platform_save_set_path(const char *path);

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
    const Result mount = fsdevMountSdmc();
    if (R_FAILED(mount)) {
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
    platform_save_set_path("/switch/earthbound-switch/earthbound.srm");

    /* The Switch SDL port selects its window renderer, audio device and
     * controller backend. The video module creates the accelerated window
     * and uploads the software PPU framebuffer every rendered frame. */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    const Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER |
                         SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(flags) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    sdl_started = true;

    if (!platform_video_init()) {
        fprintf(stderr, "platform_video_init: %s\n", SDL_GetError());
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    video_started = true;

    if (!platform_input_init()) {
        fprintf(stderr, "platform_input_init failed\n");
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    input_started = true;

    if (!platform_save_init()) {
        fprintf(stderr, "platform_save_init failed\n");
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }

    if (!platform_timer_init()) {
        fprintf(stderr, "platform_timer_init failed\n");
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    timer_started = true;

    /* Initialize game AFTER SRAM and BEFORE audio (as the Unix port does). */
    game_init();

    if (!platform_audio_init()) {
        fprintf(stderr, "platform_audio_init: %s\n", SDL_GetError());
        exit_code = EXIT_FAILURE;
        goto shutdown;
    }
    audio_started = true;
    platform_timer_frame_start();

    /* The core owns frame pacing/input polling/render dispatch inside
     * host_process_frame(). Its PPU scanlines go through platform_video_*
     * into the SDL texture. Calling host_root_boundary() at the outermost
     * loop level also preserves savestate synchronization. */
    while (appletMainLoop() && !platform_input_quit_requested()) {
        game_loop_step();
        host_process_frame();
        host_root_boundary();
    }

shutdown:
    if (audio_started) platform_audio_shutdown();
    if (timer_started) platform_timer_shutdown();
    if (input_started) platform_input_shutdown();
    if (video_started) platform_video_shutdown();
    if (sdl_started) SDL_Quit();
    if (sd_mounted) fsdevUnmountAll();
    return exit_code;
}
