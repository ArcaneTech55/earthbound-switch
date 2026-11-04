/*
 * Switch-specific EarthBound Companion SRAM backend.
 * Stores one SRAM image, preserves partial writes and maintains a backup.
 * Do not treat a successful platform_save_init() as proof of write access.
 */
#include "platform/platform.h"
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SRAM_BYTES (64u * 1024u)
static const char *save_path = "sdmc:/switch/earthbound-companion/earthbound.srm";

void platform_save_set_path(const char *path) {
    if (path && *path) save_path = path;
}

bool platform_save_init(void) {
    /* Startup may not have a save yet, but parent directory must be writable. */
    char probe[1024];
    int n = snprintf(probe, sizeof(probe), "%s.probe", save_path);
    if (n <= 0 || (size_t)n >= sizeof(probe)) return false;
    FILE *f = fopen(probe, "wb");
    if (!f) {
        fprintf(stderr, "save: cannot create test file %s (errno %d)\n",
                probe, errno);
        return false;
    }
    bool ok = (fputc(0, f) != EOF && fflush(f) == 0);
    if (fclose(f) != 0) ok = false;
    remove(probe);
    return ok;
}

static FILE *open_existing(void) {
    FILE *f = fopen(save_path, "rb");
    if (f) return f;
    /* On interrupted replacement, recover the last intact .bak. */
    char backup[1024];
    int n = snprintf(backup, sizeof(backup), "%s.bak", save_path);
    if (n <= 0 || (size_t)n >= sizeof(backup)) return NULL;
    return fopen(backup, "rb");
}

size_t platform_save_read(void *dst, size_t offset, size_t size) {
    if (!dst || !size || offset > MAX_SRAM_BYTES ||
        size > MAX_SRAM_BYTES - offset) return 0;
    FILE *f = open_existing();
    if (!f) return 0;
    if (fseek(f, (long)offset, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    size_t got = fread(dst, 1, size, f);
    fclose(f);
    return got;
}

bool platform_save_write(const void *src, size_t offset, size_t size) {
    if (!src || !size || offset > MAX_SRAM_BYTES ||
        size > MAX_SRAM_BYTES - offset) return false;
    FILE *existing = open_existing();
    size_t current_size = 0;
    if (existing) {
        if (fseek(existing, 0, SEEK_END) != 0) goto read_error;
        long length = ftell(existing);
        if (length < 0 || (unsigned long)length > MAX_SRAM_BYTES) goto read_error;
        current_size = (size_t)length;
        if (fseek(existing, 0, SEEK_SET) != 0) goto read_error;
    }
    size_t output_size = current_size > offset + size ? current_size : offset + size;
    unsigned char *data = calloc(1, output_size);
    if (!data) { if (existing) fclose(existing); return false; }
    if (existing) {
        if (fread(data, 1, current_size, existing) != current_size) {
            fclose(existing);
            free(data);
            return false;
        }
        fclose(existing);
    }
    memcpy(data + offset, src, size);
    char tmp[1024], bak[1024];
    int t = snprintf(tmp, sizeof(tmp), "%s.tmp", save_path);
    int k = snprintf(bak, sizeof(bak), "%s.bak", save_path);
    if (t <= 0 || k <= 0 || (size_t)t >= sizeof(tmp) ||
        (size_t)k >= sizeof(bak)) { free(data); return false; }
    FILE *out = fopen(tmp, "wb");
    if (!out) {
        fprintf(stderr, "save: unable to open temp file (errno %d)\n", errno);
        free(data);
        return false;
    }
    bool ok = fwrite(data, 1, output_size, out) == output_size;
    free(data);
    if (fflush(out) != 0) ok = false;
    if (fclose(out) != 0) ok = false;
    if (!ok) {
        fprintf(stderr, "save: failed writing %s (errno %d)\n", tmp, errno);
        remove(tmp);
        return false;
    }

    /* FAT/devoptab rename semantics vary. Move prior save to .bak first,
     * then promote .tmp. If replacement fails, restore from backup. */
    FILE *old = fopen(save_path, "rb");
    bool had_old = old != NULL;
    if (old) fclose(old);
    if (had_old) {
        remove(bak); /* Previous generation is no longer current. */
        if (rename(save_path, bak) != 0) {
            fprintf(stderr, "save: cannot back up existing SRAM (errno %d)\n", errno);
            remove(tmp);
            return false;
        }
    }
    if (rename(tmp, save_path) != 0) {
        int error_code = errno;
        fprintf(stderr, "save: could not install SRAM (errno %d)\n", error_code);
        if (had_old) rename(bak, save_path);
        remove(tmp);
        return false;
    }
    return true;
read_error:
    fprintf(stderr, "save: existing SRAM unreadable (errno %d)\n", errno);
    fclose(existing);
    return false;
}
