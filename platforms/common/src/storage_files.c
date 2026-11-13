#define _POSIX_C_SOURCE 200809L /* fileno, fsync */
#include "storage_files.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

void gd_file_storage_init(GdFileStorage *s, const char *dir_utf8) {
    snprintf(s->dir, sizeof(s->dir), "%s", dir_utf8);
}
static void key_path(const GdFileStorage *s, const char *key, const char *suffix, char *out, size_t cap) {
    snprintf(out, cap, "%s/%s.bin%s", s->dir, key, suffix);
}
#ifdef _WIN32
static FILE *open_utf8(const char *path, const wchar_t *mode) {
    wchar_t wide[1200];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 1200)) return NULL;
    return _wfopen(wide, mode);
}
static int replace_utf8(const char *from, const char *to) {
    wchar_t a[1200], b[1200];
    if (!MultiByteToWideChar(CP_UTF8, 0, from, -1, a, 1200) ||
        !MultiByteToWideChar(CP_UTF8, 0, to, -1, b, 1200))
        return -1;
    return MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
}
#define OPEN_READ(p)  open_utf8(p, L"rb")
#define OPEN_WRITE(p) open_utf8(p, L"wb")
#else
#include <unistd.h>
static int replace_utf8(const char *from, const char *to) { return rename(from, to); }
#define OPEN_READ(p)  fopen(p, "rb")
#define OPEN_WRITE(p) fopen(p, "wb")
#endif

int gd_file_storage_read(void *user, const char *key, uint8_t *data, size_t capacity, size_t *length) {
    char path[1200];
    key_path(user, key, "", path, sizeof(path));
    FILE *f = OPEN_READ(path);
    if (!f) return errno == ENOENT ? 1 : -1;
    size_t n = fread(data, 1, capacity, f);
    int extra = fgetc(f); /* a file larger than capacity is invalid, not truncated */
    int failed = ferror(f);
    fclose(f);
    if (failed || extra != EOF) return -1;
    *length = n;
    return 0;
}
int gd_file_storage_write(void *user, const char *key, const uint8_t *data, size_t length) {
    char path[1200], temp[1200];
    key_path(user, key, "", path, sizeof(path));
    key_path(user, key, ".tmp", temp, sizeof(temp));
    FILE *f = OPEN_WRITE(temp);
    if (!f) return -1;
    int ok = fwrite(data, 1, length, f) == length;
    ok = fflush(f) == 0 && ok;
#ifndef _WIN32
    ok = ok && fsync(fileno(f)) == 0;
#endif
    ok = fclose(f) == 0 && ok;
    if (!ok || replace_utf8(temp, path) != 0) {
        remove(temp);
        return -1;
    }
    return 0;
}
