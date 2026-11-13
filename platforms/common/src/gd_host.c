/* Gold Digger host runtime. The unchanged Passport UI modules in
   platforms/passport/main/ui are compiled against the shim declarations in
   gd_host_platform.h; this file implements those shims with host services
   and drives the UI's app_start()/app_frame(). */
#include "gd_host.h"
#include "gd_host_platform.h"
#include "../../passport/main/ui/ui.h"
#include <stdarg.h>

#ifndef GD_VERSION
#define GD_VERSION "host"
#endif

static GdHostConfig host;
static bsp_btn_cb_t host_button_cb;
static void *host_button_user;
static unsigned host_backlight_percent = 80;
static uint32_t host_framebuffer[GD_SCREEN_W * GD_SCREEN_H];
static uint8_t host_draw_buffer[GD_SCREEN_W * 40 * 2];
static bool host_frame_dirty;
static uint32_t host_random_state = 0x9e3779b9u;
static uint32_t host_usb_counter;

/* ---- logging, errors, time, randomness ---------------------------------- */
#ifdef __ANDROID__
#include <android/log.h>
#endif
void gd_host_log(char level, const char *tag, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
#ifdef __ANDROID__
    __android_log_vprint(level == 'E' ? ANDROID_LOG_ERROR : level == 'W' ? ANDROID_LOG_WARN : ANDROID_LOG_INFO,
                         tag, fmt, args);
#else
    fprintf(stderr, "%c (%s) ", level, tag);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
#endif
    va_end(args);
}
const char *esp_err_to_name(esp_err_t err) {
    switch (err) {
        case ESP_OK: return "ESP_OK";
        case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
        case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
        case ESP_ERR_INVALID_SIZE: return "ESP_ERR_INVALID_SIZE";
        case ESP_ERR_NOT_SUPPORTED: return "ESP_ERR_NOT_SUPPORTED";
        case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
        case ESP_ERR_INVALID_CRC: return "ESP_ERR_INVALID_CRC";
        case ESP_ERR_INVALID_VERSION: return "ESP_ERR_INVALID_VERSION";
        case ESP_ERR_NVS_NOT_FOUND: return "ESP_ERR_NVS_NOT_FOUND";
        default: return "ESP_FAIL";
    }
}
int64_t esp_timer_get_time(void) { return host.now_us ? host.now_us(host.user) : 0; }
static uint32_t host_tick_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
uint32_t esp_random(void) {
    uint32_t x = host_random_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return host_random_state = x;
}
const esp_app_desc_t *esp_app_get_description(void) {
    static const esp_app_desc_t desc = {GD_VERSION};
    return &desc;
}
const char *gd_host_version(void) { return GD_VERSION; }

/* ---- single-threaded FreeRTOS queue -------------------------------------- */
struct SpHostQueue {
    unsigned length, size, head, count;
    uint8_t *items;
};
QueueHandle_t xQueueCreate(unsigned length, unsigned item_size) {
    QueueHandle_t q = calloc(1, sizeof(*q));
    if (!q) return NULL;
    q->items = calloc(length, item_size);
    if (!q->items) {
        free(q);
        return NULL;
    }
    q->length = length;
    q->size = item_size;
    return q;
}
int xQueueSend(QueueHandle_t q, const void *item, int ticks) {
    (void)ticks;
    if (!q || q->count == q->length) return pdFALSE;
    memcpy(q->items + ((q->head + q->count) % q->length) * q->size, item, q->size);
    ++q->count;
    return pdTRUE;
}
int xQueueReceive(QueueHandle_t q, void *item, int ticks) {
    (void)ticks;
    if (!q || !q->count) return pdFALSE;
    memcpy(item, q->items + q->head * q->size, q->size);
    q->head = (q->head + 1) % q->length;
    --q->count;
    return pdTRUE;
}
void vTaskDelay(int ticks) { (void)ticks; }

/* ---- power: a host is always plugged in and never deep-sleeps ------------ */
int esp_sleep_get_wakeup_cause(void) { return ESP_SLEEP_WAKEUP_UNDEFINED; }
esp_err_t esp_deep_sleep_enable_gpio_wakeup(uint64_t mask, int mode) {
    (void)mask;
    (void)mode;
    return ESP_ERR_NOT_SUPPORTED;
}
void esp_deep_sleep_start(void) {}
void esp_restart(void) {}
uint32_t gd_host_usb_frame(void) { return ++host_usb_counter; }
esp_err_t bsp_button_prepare_deep_sleep(int *level) {
    if (level) *level = 1;
    return ESP_ERR_NOT_SUPPORTED;
}
int bsp_button_read_mv(void) { return 3300; }
esp_err_t bsp_battery_init(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bsp_battery_sleep(void) { return ESP_OK; }
int bsp_battery_soc(void) { return -1; }
int bsp_battery_mv(void) { return 0; }
esp_err_t bsp_battery_recover(void) { return ESP_OK; }
esp_err_t bsp_audio_sleep(void) { return ESP_OK; }
esp_err_t bsp_audio_prepare_deep_sleep(void) { return ESP_OK; }
esp_err_t bsp_i2c_prepare_deep_sleep(void) { return ESP_OK; }
esp_err_t bsp_display_prepare_deep_sleep(void) { return ESP_ERR_NOT_SUPPORTED; }

/* ---- display ------------------------------------------------------------- */
esp_err_t bsp_display_init(void) { return ESP_OK; }
void bsp_display_backlight(uint8_t percent) { host_backlight_percent = percent > 100 ? 100 : percent; }
unsigned gd_host_backlight(void) { return host_backlight_percent; }
bool bsp_lvgl_lock(int timeout_ms) {
    (void)timeout_ms;
    return true;
}
void bsp_lvgl_unlock(void) {}
static void host_flush(lv_display_t *display, const lv_area_t *area, uint8_t *data) {
    const uint16_t *src = (const uint16_t *)data;
    int width = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x) {
            uint16_t c = src[(y - area->y1) * width + (x - area->x1)];
            uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);
            host_framebuffer[y * GD_SCREEN_W + x] = host.pixels == GD_PIXELS_RGBA
                                                            ? 0xff000000u | (b << 16) | (g << 8) | r
                                                            : 0xff000000u | (r << 16) | (g << 8) | b;
        }
    host_frame_dirty = true;
    lv_display_flush_ready(display);
}
struct _lv_display_t *bsp_lvgl_init(void) {
    lv_init();
    lv_tick_set_cb(host_tick_ms);
    lv_display_t *display = lv_display_create(GD_SCREEN_W, GD_SCREEN_H);
    if (!display) return NULL;
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, host_draw_buffer, NULL, sizeof(host_draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, host_flush);
    return display;
}
const uint32_t *gd_host_framebuffer(void) { return host_framebuffer; }

/* ---- buttons: same click/long split as the Passport ADC button driver ---- */
static struct {
    bool down, long_sent;
    int64_t since;
} host_keys[3];
esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user) {
    host_button_cb = cb;
    host_button_user = user;
    return ESP_OK;
}
static void host_emit(GdKey key, bsp_btn_ev_t event) {
    if (host_button_cb) host_button_cb((bsp_btn_t)key, event, host_button_user);
}
void gd_host_key(GdKey key, bool down) {
    if ((unsigned)key > GD_KEY_OK) return;
    if (down) {
        if (host_keys[key].down) return; /* ignore OS key repeat */
        host_keys[key].down = true;
        host_keys[key].long_sent = false;
        host_keys[key].since = esp_timer_get_time();
        host_emit(key, BSP_BTN_PRESS);
        return;
    }
    if (!host_keys[key].down) return;
    host_keys[key].down = false;
    if (!host_keys[key].long_sent) host_emit(key, BSP_BTN_CLICK);
    host_emit(key, BSP_BTN_RELEASE);
}
void gd_host_key_cancel(GdKey key) {
    if ((unsigned)key > GD_KEY_OK || !host_keys[key].down) return;
    host_keys[key].down = false;
    host_emit(key, BSP_BTN_RELEASE);
}
static void host_poll_keys(void) {
    int64_t now = esp_timer_get_time();
    for (unsigned k = 0; k < 3; ++k)
        if (host_keys[k].down && !host_keys[k].long_sent &&
            now - host_keys[k].since >= (int64_t)BSP_BTN_LONG_PRESS_MS * 1000) {
            host_keys[k].long_sent = true;
            host_emit((GdKey)k, BSP_BTN_LONG);
        }
}

/* ---- storage (same keys and validation as platforms/passport/main/storage.c) */
static int host_read(const char *key, uint8_t *data, size_t capacity, size_t *length) {
    if (!host.read) return -1;
    *length = 0;
    return host.read(host.user, key, data, capacity, length);
}
static esp_err_t host_write(const char *key, const uint8_t *data, size_t length) {
    return host.write && host.write(host.user, key, data, length) == 0 ? ESP_OK : ESP_FAIL;
}
esp_err_t storage_init(void) { return host.read && host.write ? ESP_OK : ESP_ERR_NOT_SUPPORTED; }
esp_err_t storage_load(GdGame *g) {
    uint8_t *buf = malloc(GD_SAVE_BYTES);
    if (!buf) return ESP_ERR_NO_MEM;
    size_t size = 0;
    int r = host_read("save_v1", buf, GD_SAVE_BYTES, &size);
    esp_err_t err = r == 0 ? ESP_OK : r == 1 ? ESP_ERR_NVS_NOT_FOUND : ESP_FAIL;
    if (err == ESP_OK && (size > GD_SAVE_BYTES || size < 25)) err = ESP_ERR_INVALID_SIZE;
    if (err == ESP_OK && gd_save_newer(buf, size)) err = ESP_ERR_INVALID_VERSION;
    if (err == ESP_OK && !gd_save_decode(g, buf, size)) err = ESP_ERR_INVALID_CRC;
    free(buf);
    return err;
}
esp_err_t storage_save(const GdGame *g) {
    uint8_t *buf = malloc(GD_SAVE_BYTES);
    if (!buf) return ESP_ERR_NO_MEM;
    size_t size = gd_save_encode(g, buf, GD_SAVE_BYTES);
    esp_err_t err = size ? host_write("save_v1", buf, size) : ESP_ERR_INVALID_STATE;
    free(buf);
    return err;
}
void storage_preferences_load(uint8_t values[PREF_COUNT]) {
    uint8_t saved[PREF_COUNT + 1];
    size_t size = 0;
    if (host_read("preferences", saved, sizeof(saved), &size) == 0 && size == PREF_COUNT &&
        storage_preferences_valid(saved))
        memcpy(values, saved, PREF_COUNT);
}
esp_err_t storage_preferences_save(const uint8_t values[PREF_COUNT]) {
    return host_write("preferences", values, PREF_COUNT);
}

/* ---- public API ---------------------------------------------------------- */
void gd_host_pointer(GdPointer phase, int x, int y) {
    if ((unsigned)phase > GD_POINTER_CANCEL) return;
    pointer_event(x, y, (PointerPhase)phase);
}
void gd_host_scroll(int steps) { pointer_scroll(steps); }
bool gd_host_back(void) { return pointer_back(); }
bool gd_host_init(const GdHostConfig *config) {
    host = *config;
    pointer_mode = true; /* no physical buttons: tap wording, Back control */
    if (host.entropy) host_random_state = host.entropy;
    for (unsigned i = 0; i < 8; ++i) esp_random();
    if (!app_start()) return false;
    lv_refr_now(NULL);
    return true;
}
bool gd_host_tick(void) {
    host_poll_keys();
    app_frame();
    lv_timer_handler();
    bool dirty = host_frame_dirty;
    host_frame_dirty = false;
    return dirty;
}
void gd_host_save(void) {
    if (save_enabled) save_now();
}
