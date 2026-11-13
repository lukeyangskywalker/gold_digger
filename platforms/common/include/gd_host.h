/* Gold Digger host runtime: the real Passport UI (platforms/passport/main/ui,
   LVGL 240x320) plus core/, packaged for the desktop companion and Android.

   Threading: call every function except gd_host_audio_render() from one
   thread. Audio rendering may run on an audio callback thread; it is guarded
   internally. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define GD_SCREEN_W      240
#define GD_SCREEN_H      320
#define GD_AUDIO_RATE_HZ 16000

typedef enum { GD_KEY_UP = 0, GD_KEY_DOWN = 1, GD_KEY_OK = 2 } GdKey;
typedef enum {
    GD_PIXELS_ARGB = 0, /* uint32 0xAARRGGBB: Win32 DIB, Android Bitmap, SDL ARGB8888 */
    GD_PIXELS_RGBA = 1  /* bytes R,G,B,A */
} GdPixelFormat;

typedef struct {
    /* Persistent key/value storage (keys "save_v1" and "preferences"). read
       returns 0 and the length on success, 1 when the key does not exist, any
       other value on error. write must replace atomically and return 0. */
    int (*read)(void *user, const char *key, uint8_t *data, size_t capacity, size_t *length);
    int (*write)(void *user, const char *key, const uint8_t *data, size_t length);
    int64_t (*now_us)(void *user); /* monotonic microseconds */
    void *user;
    uint32_t entropy; /* seed for esp_random() */
    GdPixelFormat pixels;
} GdHostConfig;

/* Loads the save, builds the UI and renders the first frame. */
bool gd_host_init(const GdHostConfig *config);
/* One Passport main-loop iteration plus LVGL timers. Call every ~16-20 ms,
   focused or not. Returns true when the frame changed. */
bool gd_host_tick(void);
/* Physical button edges with the Passport's click/long split (500 ms). */
void gd_host_key(GdKey key, bool down);
void gd_host_key_cancel(GdKey key); /* forget a held key without a CLICK */
typedef enum { GD_POINTER_DOWN = 0, GD_POINTER_MOVE = 1, GD_POINTER_UP = 2, GD_POINTER_CANCEL = 3 } GdPointer;
/* Mouse/touch in screen pixels (0..239, 0..319). A tap activates what is drawn
   there; in the mine, tapping left/right/below the miner moves and digs. */
void gd_host_pointer(GdPointer phase, int x, int y);
void gd_host_scroll(int steps); /* wheel: positive = later rows */
/* Back (Esc, Android Back). False on the surface camp: the host may minimise. */
bool gd_host_back(void);
const uint32_t *gd_host_framebuffer(void); /* 240x320 opaque pixels */
unsigned gd_host_backlight(void);          /* 0..100 requested by the game */
void gd_host_save(void);                   /* persist now (before exit) */
void gd_host_audio_render(int16_t *out, size_t count); /* mono s16 at GD_AUDIO_RATE_HZ */
const char *gd_host_version(void);

#ifdef __cplusplus
}
#endif
