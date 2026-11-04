/*
 * Switch implementations of Companion's platform extension hooks.
 * These are separate from the ordinary Switch SDL video/input/audio backends.
 *
 * Not all desktop features are implemented on libnx. In particular, MSU1
 * PCM packs and desktop diagnostic captures are disabled explicitly rather
 * than pretending those features work. The SPC700 audio path remains active.
 */
#include "platform/platform.h"
#include <stddef.h>
#include <stdint.h>

/* No MSU1 pack is loaded by the Switch frontend. Avoid suppressing the
 * original SPC700 music whenever a high-quality track is requested. */
bool platform_audio_msu_play(uint16_t track_id) {
    (void)track_id;
    return false;
}
void platform_audio_msu_stop(void) {}
bool platform_audio_msu_is_loaded(void) { return false; }
void platform_audio_msu_mix(int16_t *out, int frames, int out_rate) {
    (void)out; (void)frames; (void)out_rate;
}
void pc_msu_command(uint8_t command) { (void)command; }

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
