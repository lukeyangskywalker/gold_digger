/* Headless check of the real Passport UI on the host runtime: boots with
   in-memory storage, drives buttons through each screen, checks the save
   round-trips, and writes 240x320 screenshots (PPM) to the given folder. */
#include "gd_host.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int64_t fake_us;
static uint8_t store_data[2][4096];
static size_t store_len[2];
static int key_index(const char *key) { return !strcmp(key, "save_v1") ? 0 : !strcmp(key, "preferences") ? 1 : -1; }
static int store_read(void *u, const char *key, uint8_t *data, size_t cap, size_t *len) {
    (void)u;
    int k = key_index(key);
    if (k < 0 || !store_len[k]) return 1;
    if (store_len[k] > cap) return -1;
    memcpy(data, store_data[k], store_len[k]);
    *len = store_len[k];
    return 0;
}
static int store_write(void *u, const char *key, const uint8_t *data, size_t len) {
    (void)u;
    int k = key_index(key);
    if (k < 0 || len > sizeof(store_data[0])) return -1;
    memcpy(store_data[k], data, len);
    store_len[k] = len;
    return 0;
}
static int64_t now(void *u) {
    (void)u;
    return fake_us;
}
static void run(unsigned ms) {
    for (unsigned t = 0; t < ms; t += 20) {
        fake_us += 20000;
        gd_host_tick();
    }
}
static void press(GdKey k, unsigned hold_ms) {
    gd_host_key(k, true);
    run(hold_ms);
    gd_host_key(k, false);
    run(120);
}
static const char *out_dir = ".";
static void shot(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", out_dir, name);
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6\n%d %d\n255\n", GD_SCREEN_W, GD_SCREEN_H);
    const uint32_t *px = gd_host_framebuffer();
    for (int i = 0; i < GD_SCREEN_W * GD_SCREEN_H; ++i) {
        uint8_t rgb[3] = {(uint8_t)(px[i] >> 16), (uint8_t)(px[i] >> 8), (uint8_t)px[i]};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("wrote %s\n", path);
}
int main(int argc, char **argv) {
    if (argc > 1) out_dir = argv[1];
    GdHostConfig c = {store_read, store_write, now, NULL, 1234, GD_PIXELS_ARGB};
    if (!gd_host_init(&c)) return 1;
    run(300);
    shot("01_home");
    press(GD_KEY_OK, 40); /* Mine Shaft: dive */
    run(200);
    shot("02_mine_surface");
    for (int i = 0; i < 6; ++i) press(GD_KEY_OK, 40);
    press(GD_KEY_DOWN, 40);
    for (int i = 0; i < 4; ++i) press(GD_KEY_OK, 40);
    run(200);
    shot("03_mine_dug");
    press(GD_KEY_OK, 700); /* hold: pause */
    shot("04_pause");
    press(GD_KEY_DOWN, 40);
    press(GD_KEY_DOWN, 40);
    press(GD_KEY_OK, 40); /* rope up */
    run(300);
    shot("05_report");
    assert(store_len[0] > 0);
    press(GD_KEY_OK, 40);
    press(GD_KEY_UP, 40); /* Trading Post */
    shot("06_home_shop");
    press(GD_KEY_OK, 40);
    shot("07_shop");
    press(GD_KEY_OK, 700);
    press(GD_KEY_DOWN, 40);
    press(GD_KEY_DOWN, 40);
    press(GD_KEY_OK, 40);
    shot("08_elevator");
    press(GD_KEY_OK, 700);
    press(GD_KEY_DOWN, 40);
    press(GD_KEY_OK, 40);
    shot("09_settings");
    gd_host_save();
    puts("host_smoke: ok");
    return 0;
}
