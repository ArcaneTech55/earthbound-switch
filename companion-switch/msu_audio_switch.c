/*
 * Native Switch MSU-1 player for Companion.
 * Compatible with the 44.1 kHz, interleaved stereo, signed-16-bit PCM packs
 * accepted by the original desktop port.  No bundled music or ROM files.
 *
 * Keep file I/O off SDL's real-time callback: track loads happen on the game
 * thread, and the callback reads from an owned buffer under a separate mutex.
 * Memory limit protects the Switch heap against accidentally huge PCM files.
 */
#include "platform/platform.h"
#include "platform/pc_options.h"
#include <SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>

#ifdef EB_ENABLE_AUDIO
#define MSU_MAX_TRACK_BYTES (64u * 1024u * 1024u)
#define MSU_RATE 44100.0

static SDL_mutex *mutex;
static char music_directory[900];
static char music_prefix[128];
static bool pack_ready;
static unsigned char *track_data;
static uint32_t track_frames, loop_frame;
static double cursor, gain = 1.0, target_gain = 1.0, fade_rate;
static bool active, repeats = true;

static void close_track_locked(void) {
    free(track_data);
    track_data = NULL;
    track_frames = loop_frame = 0;
    cursor = 0.0;
    active = false;
    gain = target_gain = 1.0;
    fade_rate = 0.0;
}

static bool extract_prefix(const char *filename, char *output, size_t capacity) {
    size_t n = strlen(filename);
    if (n < 6 || strcmp(filename + n - 4, ".pcm") != 0) return false;
    size_t i = n - 4;
    while (i > 0 && filename[i - 1] >= '0' && filename[i - 1] <= '9') --i;
    if (i == n - 4 || i < 2 || filename[i - 1] != '-') return false;
    size_t prefix_length = i - 1;
    if (prefix_length == 0 || prefix_length >= capacity) return false;
    memcpy(output, filename, prefix_length);
    output[prefix_length] = '\0';
    return true;
}

bool platform_audio_msu_autodetect_name(const char *dir, char *out, size_t capacity) {
    if (!dir || !out || !capacity) return false;
    DIR *d = opendir(dir);
    if (!d) return false;
    bool found = false;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (extract_prefix(entry->d_name, out, capacity)) {
            found = true;
            break;
        }
    }
    closedir(d);
    return found;
}

void platform_audio_msu_load(const char *dir, const char *pack_name) {
    if (!dir || !pack_name || !dir[0] || !pack_name[0]) return;
    if (!mutex) mutex = SDL_CreateMutex();
    if (!mutex) return;
    if (strlen(dir) >= sizeof(music_directory) ||
        strlen(pack_name) >= sizeof(music_prefix)) return;
    SDL_LockMutex(mutex);
    close_track_locked();
    snprintf(music_directory, sizeof(music_directory), "%s", dir);
    snprintf(music_prefix, sizeof(music_prefix), "%s", pack_name);
    pack_ready = true;
    SDL_UnlockMutex(mutex);
}

bool platform_audio_msu_is_loaded(void) {
    return pack_ready;
}

bool platform_audio_msu_play(uint16_t track_id) {
    if (!pack_ready || !mutex) return false;
    char filename[1100];
    int n = snprintf(filename, sizeof(filename), "%s/%s-%u.pcm",
                     music_directory, music_prefix, (unsigned)track_id);
    if (n <= 0 || (size_t)n >= sizeof(filename)) return false;
    FILE *f = fopen(filename, "rb");
    if (!f) return false; /* Missing track: game must fall back to SPC music. */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long length = ftell(f);
    if (length <= 8 || (unsigned long)length > MSU_MAX_TRACK_BYTES ||
        ((length - 8) % 4) != 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    size_t size = (size_t)length;
    unsigned char *data = malloc(size);
    if (!data) { fclose(f); return false; }
    size_t read_bytes = fread(data, 1, size, f);
    fclose(f);
    if (read_bytes != size || memcmp(data, "MSU1", 4) != 0) {
        free(data);
        return false;
    }
    uint32_t loop = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                    ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    uint32_t frames = (uint32_t)((size - 8) / 4);
    if (frames == 0) { free(data); return false; }

    SDL_LockMutex(mutex);
    close_track_locked();
    track_data = data;
    track_frames = frames;
    loop_frame = loop < frames ? loop : 0;
    cursor = 0;
    active = true;
    repeats = true;
    SDL_UnlockMutex(mutex);
    return true;
}

void platform_audio_msu_stop(void) {
    if (!mutex) return;
    SDL_LockMutex(mutex);
    close_track_locked();
    SDL_UnlockMutex(mutex);
}

void pc_msu_command(uint8_t command) {
    if (command == 1) { platform_audio_msu_stop(); return; }
    if (!mutex) return;
    SDL_LockMutex(mutex);
    if (command == 2) { target_gain = 0.0; fade_rate = 240.0 / 255.0; }
    if (command == 3) { target_gain = 0.0; fade_rate = 120.0 / 255.0; }
    if (command == 7) { target_gain = 64.0 / 255.0; fade_rate = 180.0 / 255.0; }
    if (command == 8) { target_gain = 1.0; fade_rate = 180.0 / 255.0; }
    SDL_UnlockMutex(mutex);
}

/* Read unaligned little-endian S16 values without assuming CPU alignment. */
static int32_t pcm_sample(uint32_t frame, int channel) {
    const unsigned char *p = track_data + 8 + ((size_t)frame * 2 + channel) * 2;
    return (int16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

void platform_audio_msu_mix(int16_t *out, int frames, int output_rate) {
    if (!mutex || !out || frames <= 0 || output_rate <= 0) return;
    SDL_LockMutex(mutex);
    if (!active || !track_data) { SDL_UnlockMutex(mutex); return; }
    double step = MSU_RATE / (double)output_rate;
    int music_volume = pc_options.music_volume;
    if (music_volume < 0) music_volume = 0;
    if (music_volume > 100) music_volume = 100;

    for (int i = 0; i < frames; ++i) {
        uint32_t j = (uint32_t)cursor;
        if (j >= track_frames) {
            if (!repeats) { active = false; break; }
            cursor = (double)loop_frame;
            j = loop_frame;
        }
        uint32_t next = j + 1;
        if (next >= track_frames) next = loop_frame;
        double fraction = cursor - (double)j;
        double delta = fade_rate / (double)output_rate;
        if (gain < target_gain) {
            gain += delta;
            if (gain > target_gain) gain = target_gain;
        } else if (gain > target_gain) {
            gain -= delta;
            if (gain < target_gain) gain = target_gain;
        }
        for (int ch = 0; ch < 2; ch++) {
            int32_t s0 = pcm_sample(j, ch);
            int32_t s1 = pcm_sample(next, ch);
            int32_t sample = (int32_t)((s0 + (s1 - s0) * fraction) *
                                       gain * music_volume / 100.0);
            int32_t mixed = (int32_t)out[i * 2 + ch] + sample;
            if (mixed > INT16_MAX) mixed = INT16_MAX;
            if (mixed < INT16_MIN) mixed = INT16_MIN;
            out[i * 2 + ch] = (int16_t)mixed;
        }
        cursor += step;
        /* Retain fractional cursor while looping (avoid cumulative drift). */
        if (cursor >= track_frames && repeats) {
            double beyond = cursor - track_frames;
            cursor = (double)loop_frame + beyond;
            if (cursor >= track_frames) cursor = (double)loop_frame;
        }
    }
    SDL_UnlockMutex(mutex);
}

void platform_audio_msu_unload(void) {
    if (!mutex) return;
    SDL_LockMutex(mutex);
    close_track_locked();
    pack_ready = false;
    SDL_UnlockMutex(mutex);
    SDL_DestroyMutex(mutex);
    mutex = NULL;
}

#else
bool platform_audio_msu_autodetect_name(const char *dir, char *out, size_t cap) {
    (void)dir; (void)out; (void)cap; return false;
}
void platform_audio_msu_load(const char *dir, const char *name) {
    (void)dir; (void)name;
}
bool platform_audio_msu_play(uint16_t id) { (void)id; return false; }
void platform_audio_msu_stop(void) {}
bool platform_audio_msu_is_loaded(void) { return false; }
void platform_audio_msu_mix(int16_t *out, int frames, int rate) {
    (void)out; (void)frames; (void)rate;
}
void pc_msu_command(uint8_t command) { (void)command; }
void platform_audio_msu_unload(void) {}
#endif
