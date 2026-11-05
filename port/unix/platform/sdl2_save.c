/*
 * Unix/SDL2 save data implementation — file-backed persistent storage.
 *
 * Save data is stored in a single file (default "earthbound.srm").
 * The file path can be overridden via --save on the command line.
 *
 * Writes are crash-safe: platform_save_write() reads the current file,
 * applies the requested change to an in-memory copy, writes the full
 * result to a temp file next to it, flushes it to stable storage, and
 * only then renames it over the live file. A crash or power loss at any
 * point leaves either the old file or the new one intact, never a
 * half-written one -- the same approach sdl2_savestate.c already uses
 * for its ping-pong slots.
 */
#include "platform/platform.h"
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>        /* _commit, _fileno */
#else
#include <unistd.h>    /* fsync, fileno, close */
#include <fcntl.h>     /* open, O_RDONLY */
#endif

static const char *save_file_path = "earthbound.srm";

#ifdef __SWITCH__
/* Keep I/O errors visible after hbmenu hides stderr. A failed write should
 * never masquerade as a successful telephone save. */
static void save_error(const char *operation) {
    int err = errno;
    FILE *log = fopen("sdmc:/switch/earthbound-switch/save_errors.log", "a");
    if (log) {
        fprintf(log, "%s: errno=%d (%s)\n", operation, err, strerror(err));
        fclose(log);
    }
}
#endif

void platform_save_set_path(const char *path) {
    save_file_path = path;
}

bool platform_save_init(void) {
#ifdef __SWITCH__
    /* Verify SD is genuinely writable before game_init attempts the SRAM
     * version check. Do not erase an existing save on a mount error. */
    char probe[1024];
    int probe_len = snprintf(probe, sizeof(probe), "%s.probe", save_file_path);
    if (probe_len <= 0 || (size_t)probe_len >= sizeof(probe)) return false;
    FILE *pf = fopen(probe, "wb");
    if (!pf) { save_error("save probe open"); return false; }
    bool ok = fputc(0x5a, pf) != EOF;
    if (fclose(pf) != 0) ok = false;
    pf = fopen(probe, "rb");
    if (!pf) ok = false;
    else { if (fgetc(pf) != 0x5a) ok = false; fclose(pf); }
    remove(probe);
    if (!ok) { save_error("save probe readback"); return false; }
    /* Recover an interrupted SD rename before the game's SRAM integrity
     * check has a chance to mistake a missing file for a fresh game. */
    if (access(save_file_path, F_OK) != 0) {
        char backup_path[1024];
        int n = snprintf(backup_path, sizeof(backup_path), "%s.bak", save_file_path);
        if (n <= 0 || (size_t)n >= sizeof(backup_path)) return false;
        if (access(backup_path, F_OK) == 0 && rename(backup_path, save_file_path) != 0) {
            save_error("restore SRAM backup");
            return false;
        }
    }
#endif
    /* Nothing to initialize for file-backed saves */
    return true;
}

size_t platform_save_read(void *dst, size_t offset, size_t size) {
    FILE *f = fopen(save_file_path, "rb");
    if (!f) return 0;

    if (fseek(f, (long)offset, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }

    size_t read = fread(dst, 1, size, f);
    fclose(f);
    return read;
}

/* "<save_file_path>.tmp" -- staged next to the real file so the final
 * rename() is same-directory (required for it to be atomic). */
static bool make_tmp_path(char *out, size_t out_size) {
    int n = snprintf(out, out_size, "%s.tmp", save_file_path);
    return n > 0 && (size_t)n < out_size;
}

#ifndef _WIN32
/* Best-effort: fsync the directory holding save_file_path so the rename
 * itself is durable, not just the file contents. A failure here doesn't
 * undo the write -- the rename already landed on disk by this point,
 * this only protects the directory entry against a second, immediately
 * following crash. */
static void fsync_containing_dir(void) {
    char dir[1024];
    strncpy(dir, save_file_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';

    char *slash = strrchr(dir, '/');
    const char *dir_path = ".";
    if (slash) {
        *slash = '\0';
        if (dir[0] != '\0') dir_path = dir;
    }

    int fd = open(dir_path, O_RDONLY);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
}
#endif

bool platform_save_write(const void *src, size_t offset, size_t size) {
    if (!src || size == 0 || offset > SIZE_MAX - size) return false;
    /* Read whatever's on disk now so a write that only touches one
     * SaveBlock copy (or the 2-byte version word) doesn't lose the rest
     * of the file -- callers always write a sub-range, never the whole
     * image in one call. */
    size_t existing = 0;
    FILE *rf = fopen(save_file_path, "rb");
    if (rf) {
        if (fseek(rf, 0, SEEK_END) == 0) {
            long len = ftell(rf);
            if (len > 0) existing = (size_t)len;
        }
        fseek(rf, 0, SEEK_SET);
    }

    size_t buf_size = existing > offset + size ? existing : offset + size;
    unsigned char *buf = calloc(1, buf_size);
    if (!buf) {
        if (rf) fclose(rf);
        return false;
    }
    if (rf) {
        fread(buf, 1, existing, rf);
        fclose(rf);
    }
    memcpy(buf + offset, src, size);

    char tmp_path[1024];
    if (!make_tmp_path(tmp_path, sizeof(tmp_path))) {
        free(buf);
        return false;
    }

    FILE *wf = fopen(tmp_path, "wb");
    if (!wf) {
#ifdef __SWITCH__
        save_error("open SRAM temporary file");
#endif
        free(buf);
        return false;
    }
    size_t written = fwrite(buf, 1, buf_size, wf);
    free(buf);

    bool ok = (written == buf_size) && (fflush(wf) == 0);
#ifdef _WIN32
    if (ok && _commit(_fileno(wf)) != 0) /* durability: the new file must hit stable storage */
        ok = false;
#elif !defined(__SWITCH__)
    if (ok && fsync(fileno(wf)) != 0) /* durability: the new file must hit stable storage */
        ok = false;
#endif
    if (fclose(wf) != 0)
        ok = false;
    if (!ok) {
#ifdef __SWITCH__
        save_error("write SRAM temporary file");
#endif
        remove(tmp_path);
        return false;
    }

#ifdef _WIN32
    /* POSIX rename() atomically replaces an existing destination; the
     * MSVC/mingw C runtime's rename() does not, so the old file has to be
     * removed first. A crash between remove() and rename() would lose the
     * previous save on Windows specifically -- still strictly better than
     * the old in-place write, which could tear mid-write on every
     * platform (Windows included), corrupting the live save outright
     * rather than, in the worst case, losing one generation of it. */
    remove(save_file_path);
#endif
    if (rename(tmp_path, save_file_path) != 0) {
#ifdef __SWITCH__
        /* Some Switch SD filesystem implementations refuse to rename over an
         * existing destination. Keep the old image as a recovery copy until
         * the new image is installed; never unlink the only valid save. */
        char backup_path[1024];
        int n = snprintf(backup_path, sizeof(backup_path), "%s.bak", save_file_path);
        if (n > 0 && (size_t)n < sizeof(backup_path)) {
            /* Do not discard a prior recovery copy if the live save is absent. */
            if (access(save_file_path, F_OK) == 0) {
                /* A stale backup is only replaced while a live save exists. */
                remove(backup_path);
                if (rename(save_file_path, backup_path) == 0) {
                    if (rename(tmp_path, save_file_path) == 0) {
                        remove(backup_path);
                    goto verify_write;
                    }
                    rename(backup_path, save_file_path);
                }
            }
        }
#endif
#ifdef __SWITCH__
        save_error("install SRAM file");
#endif
        remove(tmp_path);
        return false;
    }

#if !defined(_WIN32) && !defined(__SWITCH__)
    fsync_containing_dir();
#endif
#ifdef __SWITCH__
verify_write:
    /* Reopen the file after close/rename. This catches silent SD write errors
     * immediately instead of leaving the file menu to show an empty slot. */
    {
        FILE *check = fopen(save_file_path, "rb");
        if (!check) { save_error("reopen saved SRAM"); return false; }
        bool verified = fseek(check, (long)offset, SEEK_SET) == 0;
        const unsigned char *expected = (const unsigned char *)src;
        unsigned char chunk[256];
        size_t remaining = size;
        while (verified && remaining) {
            size_t n = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
            if (fread(chunk, 1, n, check) != n || memcmp(chunk, expected, n) != 0) {
                verified = false;
                break;
            }
            expected += n;
            remaining -= n;
        }
        fclose(check);
        if (!verified) { save_error("SRAM readback mismatch"); return false; }
    }
#endif
    return true;
}
