/* Host implementation of platforms/passport/main/game_audio.h: the same
   synthesizer as the Passport, pulled by the platform audio callback through
   gd_host_audio_render(). */
#include "game_audio.h"
#include "gd_host.h"
#include <stdatomic.h>
#include <string.h>
_Static_assert(GD_AUDIO_RATE_HZ == GD_SYNTH_RATE, "host output runs at the synth rate");

static atomic_flag lock = ATOMIC_FLAG_INIT;
static void enter(void) {
    while (atomic_flag_test_and_set_explicit(&lock, memory_order_acquire)) {}
}
static void leave(void) { atomic_flag_clear_explicit(&lock, memory_order_release); }
static GdSynth synth;
static int started;

void game_audio_start(uint8_t music, uint8_t effects) {
    enter();
    if (!started) gd_synth_init(&synth);
    started = 1;
    gd_synth_levels(&synth, music, effects);
    leave();
}
void game_audio_levels(uint8_t music, uint8_t effects) {
    enter();
    gd_synth_levels(&synth, music, effects);
    leave();
}
void game_audio_music(GdMusic music) {
    enter();
    gd_synth_music(&synth, music);
    leave();
}
void game_audio_effect(GdSound sound) {
    enter();
    gd_synth_effect(&synth, sound);
    leave();
}
esp_err_t game_audio_stop(void) { return ESP_OK; }
void gd_host_audio_render(int16_t *out, size_t count) {
    enter();
    if (started)
        gd_synth_render(&synth, out, count);
    else
        memset(out, 0, count * sizeof(*out));
    leave();
}
