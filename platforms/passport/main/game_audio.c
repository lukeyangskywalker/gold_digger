/* Passport audio task: renders gd_synth into 32-bit I2S frames at 16 kHz. The
   UI only queues commands, so a slow codec write never blocks input. */
#include "game_audio.h"
#include <stdbool.h>
#include <string.h>
#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

typedef struct {
    uint8_t kind, a, b;
} AudioCommand;
enum { AUDIO_LEVELS, AUDIO_MUSIC, AUDIO_EFFECT, AUDIO_STOP };
static QueueHandle_t commands;
static SemaphoreHandle_t stopped;
static const char *TAG = "game_audio";
static int16_t pcm16[320]; /* static: keep buffers off the task stack (no PSRAM) */
static int32_t pcm32[320];

static void audio_task(void *unused) {
    (void)unused;
    static GdSynth synth;
    gd_synth_init(&synth);
    bool ready = bsp_audio_init() == ESP_OK && bsp_audio_set_format(GD_SYNTH_RATE, 32, 1) == ESP_OK;
    if (!ready) ESP_LOGW(TAG, "Audio unavailable; game continues silently");
    bool running = true;
    while (running) {
        AudioCommand c;
        while (xQueueReceive(commands, &c, 0) == pdTRUE) {
            if (c.kind == AUDIO_LEVELS) {
                /* The codec runs at the louder setting; the synth scales the quieter one. */
                unsigned hw = c.a > c.b ? c.a : c.b;
                bsp_audio_set_volume((uint8_t)hw);
                gd_synth_levels(&synth, hw ? c.a * 100u / hw : 0, hw ? c.b * 100u / hw : 0);
            } else if (c.kind == AUDIO_MUSIC)
                gd_synth_music(&synth, (GdMusic)c.a);
            else if (c.kind == AUDIO_EFFECT)
                gd_synth_effect(&synth, (GdSound)c.a);
            else if (c.kind == AUDIO_STOP)
                running = false;
        }
        if (!running) break;
        if (!ready) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        gd_synth_render(&synth, pcm16, 320);
        for (unsigned i = 0; i < 320; ++i) pcm32[i] = (int32_t)pcm16[i] * 65536;
        if (bsp_audio_write(pcm32, sizeof(pcm32)) != ESP_OK) {
            ESP_LOGW(TAG, "Audio playback stopped");
            ready = false;
        }
    }
    xSemaphoreGive(stopped);
    vTaskDelete(NULL);
}
static void send(unsigned kind, unsigned a, unsigned b) {
    if (!commands) return;
    AudioCommand c = {(uint8_t)kind, (uint8_t)a, (uint8_t)b};
    xQueueSend(commands, &c, 0);
}
void game_audio_start(uint8_t music, uint8_t effects) {
    if (commands) return;
    commands = xQueueCreate(16, sizeof(AudioCommand));
    stopped = xSemaphoreCreateBinary();
    if (!commands || !stopped || xTaskCreate(audio_task, "game_audio", 3072, NULL, 5, NULL) != pdPASS) {
        if (commands) vQueueDelete(commands);
        if (stopped) vSemaphoreDelete(stopped);
        commands = NULL;
        stopped = NULL;
        return;
    }
    game_audio_levels(music, effects);
}
void game_audio_levels(uint8_t music, uint8_t effects) { send(AUDIO_LEVELS, music, effects); }
void game_audio_music(GdMusic music) {
    static int previous = -1;
    if ((int)music == previous) return;
    previous = (int)music;
    send(AUDIO_MUSIC, music, 0);
}
void game_audio_effect(GdSound sound) { send(AUDIO_EFFECT, sound, 0); }
esp_err_t game_audio_stop(void) {
    if (!commands) return ESP_OK;
    AudioCommand c = {AUDIO_STOP, 0, 0};
    if (xQueueSend(commands, &c, pdMS_TO_TICKS(100)) != pdTRUE ||
        xSemaphoreTake(stopped, pdMS_TO_TICKS(2000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    vQueueDelete(commands);
    vSemaphoreDelete(stopped);
    commands = NULL;
    stopped = NULL;
    return ESP_OK;
}
