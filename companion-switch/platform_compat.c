/*
 * Switch implementations of Companion's platform extension hooks.
 * These are separate from the ordinary Switch SDL video/input/audio backends.
 *
 * PC-only diagnostic captures are not implemented. MSU-1 playback is supplied
 * separately by companion_msu_audio.c, leaving this file for non-audio hooks.
 */
#include "platform/platform.h"
#include <stddef.h>
#include <stdint.h>

/* The SDL Switch renderer is not the desktop post-processing renderer.
 * The standard frame presentation is still driven by its end_frame hook. */
void platform_video_request_motion_dump(int frames) { (void)frames; }
bool platform_video_motion_dump_active(void) { return false; }
void platform_video_request_ppu_dump(void) {}
void platform_video_request_mark_screenshot(void) {}
void platform_video_set_zoom(EbZoomMode mode) { (void)mode; }
void platform_video_set_fx_suppressed(bool suppressed) { (void)suppressed; }
void platform_video_set_dof_suppressed(bool suppressed) { (void)suppressed; }
void platform_video_set_static_screen(bool is_static_screen) {
    (void)is_static_screen;
}
void platform_log_set_enabled(bool enabled) { (void)enabled; }

/* Temporary, aligned compressor scratch. The core requires >= 512 bytes;
 * 32 KiB allows the 16 KiB streaming stage plus compression workspace.
 * Savestate serialization is invoked at root boundaries, not concurrently. */
static union {
    uint64_t aligned;
    unsigned char data[32768];
} savestate_scratch;
void *platform_savestate_scratch(size_t *out_bytes) {
    if (out_bytes) *out_bytes = sizeof(savestate_scratch.data);
    return savestate_scratch.data;
}
