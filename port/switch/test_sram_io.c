/* Host regression test for the same SRAM backend used by the Switch port. */
#include "platform/platform.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void platform_save_set_path(const char *path);

int main(void) {
    char dir[] = "/tmp/eb-switch-sram-XXXXXX";
    assert(mkdtemp(dir));
    char path[256];
    snprintf(path, sizeof(path), "%s/earthbound.srm", dir);
    platform_save_set_path(path);
    assert(platform_save_init());
    unsigned char first[1280], second[1280], out[1280];
    memset(first, 0x5a, sizeof(first));
    memset(second, 0xa5, sizeof(second));
    assert(platform_save_write(first, 0, sizeof(first)));
    assert(platform_save_write(second, sizeof(first), sizeof(second)));
    assert(platform_save_read(out, 0, sizeof(out)) == sizeof(out));
    assert(memcmp(first, out, sizeof(out)) == 0);
    assert(platform_save_read(out, sizeof(first), sizeof(out)) == sizeof(out));
    assert(memcmp(second, out, sizeof(out)) == 0);
    assert(platform_save_write(second, 0, sizeof(second)));
    assert(platform_save_read(out, 0, sizeof(out)) == sizeof(out));
    assert(memcmp(second, out, sizeof(out)) == 0);
    assert(!platform_save_write(first, (size_t)-16, sizeof(first)));
    unlink(path);
    rmdir(dir);
    return 0;
}
