/* Host regression test for the same SRAM backend used by the Switch port. */
#include "platform/platform.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern void platform_save_set_path(const char *path);

#ifdef EB_TEST_NO_REPLACE_RENAME
/* Simulate a filesystem that refuses rename(old, existing_destination),
 * as some SD devoptab implementations do. */
extern int __real_rename(const char *, const char *);
int __wrap_rename(const char *from, const char *to) {
    if (access(to, F_OK) == 0) { errno = EEXIST; return -1; }
    return __real_rename(from, to);
}
#endif

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--verify") == 0) {
        platform_save_set_path(argv[2]);
        assert(platform_save_init());
        unsigned char check[1280];
        assert(platform_save_read(check, 0, sizeof(check)) == sizeof(check));
        for (size_t i = 0; i < sizeof(check); ++i) assert(check[i] == 0xa5);
        assert(platform_save_read(check, 1280, sizeof(check)) == sizeof(check));
        for (size_t i = 0; i < sizeof(check); ++i) assert(check[i] == 0xa5);
        return 0;
    }
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
    /* Verify the actual bytes persist after a fresh process initializes the
     * save backend, rather than merely observing one process's memory. */
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        execl(argv[0], argv[0], "--verify", path, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    /* Simulate power loss after moving live SRAM to .bak but before a new
     * file is installed: the next startup must restore the backup. */
    char backup[280];
    snprintf(backup, sizeof(backup), "%s.bak", path);
    assert(rename(path, backup) == 0);
    assert(platform_save_init());
    assert(platform_save_read(out, 0, sizeof(out)) == sizeof(out));
    assert(memcmp(second, out, sizeof(out)) == 0);

    /* A malformed oversized SRAM file must never be silently truncated. */
    FILE *oversize = fopen(path, "r+b");
    assert(oversize);
    assert(fseek(oversize, 8192, SEEK_SET) == 0);
    assert(fputc(0x7f, oversize) != EOF);
    assert(fclose(oversize) == 0);
    assert(!platform_save_write(first, 0, sizeof(first)));
    oversize = fopen(path, "rb");
    assert(oversize);
    assert(fseek(oversize, 0, SEEK_END) == 0);
    assert(ftell(oversize) == 8193);
    fclose(oversize);
    unlink(path);
    rmdir(dir);
    return 0;
}
