/* Game audio front end. Passport: a FreeRTOS task feeding the ES8311 codec
   (game_audio.c). Hosts: platforms/common/src/host_audio.c, pulled by the
   platform's audio callback. */
#pragma once
#include <stdint.h>
#include "gd_synth.h"
#include "esp_err.h"
void game_audio_start(uint8_t music_percent, uint8_t effect_percent);
void game_audio_levels(uint8_t music_percent, uint8_t effect_percent);
void game_audio_music(GdMusic music);
void game_audio_effect(GdSound sound);
esp_err_t game_audio_stop(void); /* before deep sleep */
