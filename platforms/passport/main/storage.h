/* Persistent storage. Passport: private save log in partition "gd_save" (storage.c).
   Hosts: platforms/common/src/gd_host.c with the same keys and validation. */
#pragma once
#include "digger.h"
#include "esp_err.h"
enum { PREF_MUSIC, PREF_EFFECTS, PREF_BRIGHTNESS, PREF_SLEEP, PREF_COUNT };
esp_err_t storage_init(void);
/* ESP_ERR_INVALID_VERSION: the save is from a newer firmware and is left untouched. */
esp_err_t storage_load(GdGame *g);
esp_err_t storage_save(const GdGame *g);
/* Missing or invalid preferences leave the defaults in values. */
void storage_preferences_load(uint8_t values[PREF_COUNT]);
esp_err_t storage_preferences_save(const uint8_t values[PREF_COUNT]);
/* UI language index; 0xff when never chosen. Callers range-check it. */
uint8_t storage_language_load(void);
esp_err_t storage_language_save(uint8_t language);
/* Shared validation for both backends. */
static inline int storage_preferences_valid(const uint8_t v[PREF_COUNT]) {
    return v[PREF_MUSIC] <= 100 && v[PREF_EFFECTS] <= 100 && v[PREF_BRIGHTNESS] >= 10 &&
           v[PREF_BRIGHTNESS] <= 100 && v[PREF_SLEEP] <= 4;
}
