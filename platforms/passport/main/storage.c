/* Passport storage: a private flash partition ("gd_save", see partitions.csv)
   holding an append-only log of self-checking records. Each record carries
   the whole persistent state (game save, preferences, language).

   Why not NVS: other store apps share flash with us and may leave anything
   behind, including a full or foreign NVS partition that refuses to open.
   Here, bytes that are not a valid Gold Digger record (magic + CRC) are simply
   treated as free space, so a downloaded Gold Digger can always save.

   Our own data is still protected: a record whose format or game schema is
   newer than this firmware makes storage_load() fail with
   ESP_ERR_INVALID_VERSION and the UI turns saving off, so it is never
   overwritten. A damaged latest record falls back to the previous one, since
   records are appended and old blocks are erased only when the log wraps.

   Saves made by earlier builds in the shared NVS partition "game_data" are
   imported once, read-only; that partition is never written or erased. */
#include "storage.h"
#include "esp_partition.h"
#include "esp_rom_crc.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>

#define MAGIC     0x31534447u /* "GDS1" */
#define FORMAT    1u
#define BLOCK     8192u       /* erase unit: one record never crosses a block */
#define ALIGN(n)  (((n) + 15u) & ~15u)

typedef struct {
    uint32_t magic, seq;
    uint16_t format, save_len;
    uint8_t prefs[PREF_COUNT], language, has_prefs;
    uint8_t pad[10];
    uint32_t crc; /* over the header up to here and the payload */
} Header;
_Static_assert(sizeof(Header) == 32, "record header layout");

typedef struct {
    uint32_t seq;
    uint16_t save_len;
    uint8_t prefs[PREF_COUNT], language, has_prefs;
    bool newer;
    uint8_t save[GD_SAVE_BYTES];
} State;

static const char *TAG_S = "storage";
static const esp_partition_t *part;
static State *state;      /* latest valid record (or imported legacy data) */
static bool have_state;   /* a record or legacy data was found */
static uint32_t write_at; /* next append offset; UINT32_MAX = start a fresh block */

static uint32_t record_crc(const Header *h, const uint8_t *payload) {
    uint32_t crc = esp_rom_crc32_le(0, (const uint8_t *)h, offsetof(Header, crc));
    return esp_rom_crc32_le(crc, payload, h->save_len);
}
static bool all_erased(uint32_t offset, uint32_t len) {
    uint8_t buf[64];
    while (len) {
        uint32_t n = len < sizeof(buf) ? len : sizeof(buf);
        if (esp_partition_read(part, offset, buf, n) != ESP_OK) return false;
        for (uint32_t i = 0; i < n; ++i)
            if (buf[i] != 0xff) return false;
        offset += n;
        len -= n;
    }
    return true;
}

/* Scan every block; keep the valid record with the highest sequence number. */
static void scan(void) {
    uint8_t *payload = malloc(GD_SAVE_BYTES);
    if (!payload) return;
    uint32_t best_end = UINT32_MAX;
    for (uint32_t block = 0; block + BLOCK <= part->size; block += BLOCK) {
        for (uint32_t at = block; at + sizeof(Header) <= block + BLOCK;) {
            Header h;
            if (esp_partition_read(part, at, &h, sizeof(h)) != ESP_OK || h.magic != MAGIC) break;
            if (h.save_len > GD_SAVE_BYTES || at + ALIGN(sizeof(h) + h.save_len) > block + BLOCK) break;
            if (esp_partition_read(part, at + sizeof(h), payload, h.save_len) != ESP_OK) break;
            if (record_crc(&h, payload) != h.crc) break; /* damaged: nothing after it in this block is trusted */
            uint32_t end = at + ALIGN(sizeof(h) + h.save_len);
            if (!have_state || h.seq > state->seq) {
                have_state = true;
                state->seq = h.seq;
                state->newer = h.format > FORMAT;
                state->save_len = h.save_len;
                memcpy(state->save, payload, h.save_len);
                memcpy(state->prefs, h.prefs, PREF_COUNT);
                state->language = h.language;
                state->has_prefs = h.has_prefs;
                best_end = end;
            }
            at = end;
        }
    }
    free(payload);
    write_at = best_end;
}

/* Earlier builds kept their data in NVS "game_data". Read it if it opens. */
static void import_legacy(void) {
    if (nvs_flash_init_partition("game_data") != ESP_OK) return;
    nvs_handle_t h;
    if (nvs_open_from_partition("game_data", "golddigger", NVS_READONLY, &h) == ESP_OK) {
        size_t size = GD_SAVE_BYTES;
        if (nvs_get_blob(h, "save_v1", state->save, &size) == ESP_OK) {
            state->save_len = (uint16_t)size;
            have_state = true;
        }
        size = PREF_COUNT;
        if (nvs_get_blob(h, "preferences", state->prefs, &size) == ESP_OK && size == PREF_COUNT) {
            state->has_prefs = 1;
            have_state = true;
        }
        if (nvs_get_u8(h, "language", &state->language) == ESP_OK) have_state = true;
        nvs_close(h);
    }
    nvs_flash_deinit_partition("game_data");
    if (have_state) ESP_LOGI(TAG_S, "Imported the save from the shared NVS partition");
}

esp_err_t storage_init(void) {
    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "gd_save");
    if (!part || part->size < 2 * BLOCK) return ESP_ERR_NOT_FOUND;
    state = calloc(1, sizeof(State));
    if (!state) return ESP_ERR_NO_MEM;
    state->language = 0xff;
    scan();
    if (!have_state) import_legacy();
    return ESP_OK;
}

/* Append the current state as a new record. Blocks are erased only when the
   log moves into them, never the block that holds the latest record. */
static esp_err_t append(void) {
    if (!part || !state) return ESP_ERR_INVALID_STATE;
    if (state->newer) return ESP_ERR_INVALID_VERSION;
    Header h = {.magic = MAGIC, .seq = state->seq + 1, .format = FORMAT, .save_len = state->save_len,
                .language = state->language, .has_prefs = state->has_prefs};
    memcpy(h.prefs, state->prefs, PREF_COUNT);
    memset(h.pad, 0xff, sizeof(h.pad));
    h.crc = record_crc(&h, state->save);
    uint32_t len = ALIGN(sizeof(h) + h.save_len);
    uint32_t at = write_at;
    bool fresh = at == UINT32_MAX || at / BLOCK != (at + len - 1) / BLOCK || at + len > part->size ||
                 !all_erased(at, len);
    if (fresh) {
        at = at == UINT32_MAX ? 0 : (at / BLOCK + 1) * BLOCK;
        if (at + BLOCK > part->size) at = 0;
        esp_err_t err = esp_partition_erase_range(part, at, BLOCK);
        if (err != ESP_OK) return err;
    }
    uint8_t *buf = malloc(len);
    if (!buf) return ESP_ERR_NO_MEM;
    memset(buf, 0xff, len);
    memcpy(buf, &h, sizeof(h));
    memcpy(buf + sizeof(h), state->save, h.save_len);
    esp_err_t err = esp_partition_write(part, at, buf, len);
    if (err == ESP_OK) { /* read back: only a verified record counts */
        Header check;
        err = esp_partition_read(part, at, &check, sizeof(check));
        if (err == ESP_OK && memcmp(&check, &h, sizeof(h))) err = ESP_ERR_INVALID_CRC;
    }
    free(buf);
    if (err != ESP_OK) {
        write_at = at / BLOCK * BLOCK + BLOCK - 16; /* the next append starts a fresh block */
        return err;
    }
    state->seq = h.seq;
    write_at = at + len;
    have_state = true;
    return ESP_OK;
}

esp_err_t storage_load(GdGame *g) {
    if (!state) return ESP_ERR_INVALID_STATE;
    if (!have_state || !state->save_len) return ESP_ERR_NVS_NOT_FOUND;
    if (state->newer || gd_save_newer(state->save, state->save_len)) {
        state->newer = true;
        return ESP_ERR_INVALID_VERSION;
    }
    return gd_save_decode(g, state->save, state->save_len) ? ESP_OK : ESP_ERR_INVALID_CRC;
}
esp_err_t storage_save(const GdGame *g) {
    if (!state) return ESP_ERR_INVALID_STATE;
    size_t size = gd_save_encode(g, state->save, GD_SAVE_BYTES);
    if (!size) return ESP_ERR_INVALID_STATE;
    state->save_len = (uint16_t)size;
    return append();
}
void storage_preferences_load(uint8_t values[PREF_COUNT]) {
    if (state && state->has_prefs && storage_preferences_valid(state->prefs)) memcpy(values, state->prefs, PREF_COUNT);
}
esp_err_t storage_preferences_save(const uint8_t values[PREF_COUNT]) {
    if (!state) return ESP_ERR_INVALID_STATE;
    memcpy(state->prefs, values, PREF_COUNT);
    state->has_prefs = 1;
    return append();
}
uint8_t storage_language_load(void) { return state ? state->language : 0xff; }
esp_err_t storage_language_save(uint8_t language) {
    if (!state) return ESP_ERR_INVALID_STATE;
    state->language = language;
    return append();
}
