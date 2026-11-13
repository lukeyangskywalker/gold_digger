/* File-backed key/value storage for GdHostConfig (desktop and Android).
   Each key is <dir>/<key>.bin, replaced atomically via a temporary file. */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct {
    char dir[1024]; /* UTF-8 */
} GdFileStorage;
void gd_file_storage_init(GdFileStorage *s, const char *dir_utf8);
int gd_file_storage_read(void *user, const char *key, uint8_t *data, size_t capacity, size_t *length);
int gd_file_storage_write(void *user, const char *key, const uint8_t *data, size_t length);
