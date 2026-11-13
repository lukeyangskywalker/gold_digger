#include "storage.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>

/* NVS commits atomically. Never erase the partition after a load or init error:
   a newer or damaged save stays on flash for a later firmware to read. */
#define NS "golddigger"
esp_err_t storage_init(void) {
    esp_err_t err = nvs_flash_init();
    return err == ESP_OK ? nvs_flash_init_partition("game_data") : err;
}
static esp_err_t open_ns(nvs_open_mode_t mode, nvs_handle_t *h) {
    return nvs_open_from_partition("game_data", NS, mode, h);
}
esp_err_t storage_load(GdGame *g) {
    nvs_handle_t h;
    esp_err_t err = open_ns(NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t size = 0;
    err = nvs_get_blob(h, "save_v1", NULL, &size);
    if (err == ESP_OK && (size > GD_SAVE_BYTES || size < 25)) err = ESP_ERR_INVALID_SIZE;
    uint8_t *buf = err == ESP_OK ? malloc(size) : NULL;
    if (err == ESP_OK && !buf) err = ESP_ERR_NO_MEM;
    if (err == ESP_OK) err = nvs_get_blob(h, "save_v1", buf, &size);
    nvs_close(h);
    if (err == ESP_OK && gd_save_newer(buf, size)) err = ESP_ERR_INVALID_VERSION;
    if (err == ESP_OK && !gd_save_decode(g, buf, size)) err = ESP_ERR_INVALID_CRC;
    free(buf);
    return err;
}
static esp_err_t put(const char *key, const void *data, size_t size) {
    nvs_handle_t h;
    esp_err_t err = open_ns(NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, key, data, size);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
esp_err_t storage_save(const GdGame *g) {
    uint8_t *buf = malloc(GD_SAVE_BYTES);
    if (!buf) return ESP_ERR_NO_MEM;
    size_t size = gd_save_encode(g, buf, GD_SAVE_BYTES);
    esp_err_t err = size ? put("save_v1", buf, size) : ESP_ERR_INVALID_STATE;
    free(buf);
    return err;
}
void storage_preferences_load(uint8_t values[PREF_COUNT]) {
    nvs_handle_t h;
    uint8_t saved[PREF_COUNT];
    size_t size = sizeof(saved);
    if (open_ns(NVS_READONLY, &h) != ESP_OK) return;
    if (nvs_get_blob(h, "preferences", saved, &size) == ESP_OK && size == PREF_COUNT &&
        storage_preferences_valid(saved))
        memcpy(values, saved, PREF_COUNT);
    nvs_close(h);
}
esp_err_t storage_preferences_save(const uint8_t values[PREF_COUNT]) {
    return put("preferences", values, PREF_COUNT);
}
uint8_t storage_language_load(void) {
    nvs_handle_t h;
    uint8_t value = 0xff;
    if (open_ns(NVS_READONLY, &h) != ESP_OK) return 0xff;
    if (nvs_get_u8(h, "language", &value) != ESP_OK) value = 0xff;
    nvs_close(h);
    return value;
}
esp_err_t storage_language_save(uint8_t language) {
    nvs_handle_t h;
    esp_err_t err = open_ns(NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, "language", language);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
