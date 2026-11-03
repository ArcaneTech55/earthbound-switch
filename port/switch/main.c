#include <SDL.h>
#include <switch.h>
#include <stdio.h>
#include <sys/stat.h>
#include "platform/platform.h"
#include "game_main.h"

/* File-backed SRAM implementation reused from the desktop port. */
extern void platform_save_set_path(const char *path);

static void shutdown_platform(void) {
    platform_audio_shutdown();
    platform_timer_shutdown();
    platform_input_shutdown();
    platform_video_shutdown();
    SDL_Quit();
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    /* libnx's SD filesystem provider is required for persistent SRAM. */
    Result mount_result=fsdevMountSdmc();
    if (R_FAILED(mount_result)) return 1;
    (void)mkdir("sdmc:/switch",0777);
    (void)mkdir("sdmc:/switch/earthbound-switch",0777);
    platform_save_set_path("sdmc:/switch/earthbound-switch/earthbound.srm");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER |
                 SDL_INIT_GAMECONTROLLER) != 0) {
        fsdevUnmountAll();
        return 2;
    }
    if (!platform_video_init()) { shutdown_platform();fsdevUnmountAll();return 3; }
    if (!platform_input_init()) { shutdown_platform();fsdevUnmountAll();return 4; }
    if (!platform_save_init()) { shutdown_platform();fsdevUnmountAll();return 5; }
    if (!platform_timer_init()) { shutdown_platform();fsdevUnmountAll();return 6; }

    game_init();
    if (!platform_audio_init()) {
        /* Audio is optional in early port bring-up; game can still run. */
        SDL_Log("EarthBound: audio could not start");
    }

    platform_timer_frame_start();
    while (appletMainLoop() && !platform_input_quit_requested()) {
        game_loop_step();
        host_process_frame();
        host_root_boundary();
    }
    shutdown_platform();
    fsdevUnmountAll();
    return 0;
}
