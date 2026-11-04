/* Persistent Companion engine settings on the Nintendo Switch SD card. */
#include "platform/platform.h"
#include <stdio.h>
#include <string.h>

static const char *const path = "sdmc:/switch/earthbound-companion/settings.dat";

size_t platform_settings_read(void *dst, size_t size) {
    if (!dst || !size) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t got = fread(dst, 1, size, f);
    fclose(f);
    return got;
}

bool platform_settings_write(const void *src, size_t size) {
    if (!src || !size || size > 4096) return false;
    char tmp[160];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n <= 0 || (size_t)n >= sizeof(tmp)) return false;
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    size_t written = fwrite(src, 1, size, f);
    bool ok = (written == size && fflush(f) == 0);
    if (fclose(f) != 0) ok = false;
    if (!ok) { remove(tmp); return false; }
    /* SD filesystem: replace existing file if rename() won't overwrite. */
    if (rename(tmp, path) != 0) {
        remove(path);
        if (rename(tmp, path) != 0) { remove(tmp); return false; }
    }
    return true;
}
