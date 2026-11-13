/* Small original chiptune synthesizer: two music loops and the game's sound
   effects, rendered as mono 16-bit PCM at GD_SYNTH_RATE. Shared unchanged by
   the Passport audio task and every host port. */
#pragma once
#include <stddef.h>
#include <stdint.h>

#define GD_SYNTH_RATE 16000u

typedef enum { GD_MUSIC_SURFACE, GD_MUSIC_MINE, GD_MUSIC_COUNT } GdMusic;
typedef enum {
    GD_SOUND_NONE,
    GD_SOUND_MENU,
    GD_SOUND_DIG,
    GD_SOUND_ORE,
    GD_SOUND_CHEST,
    GD_SOUND_GAS,
    GD_SOUND_HURT,
    GD_SOUND_BLAST,
    GD_SOUND_COINS,
    GD_SOUND_DENY,
    GD_SOUND_COUNT
} GdSound;

typedef struct {
    uint32_t position; /* music sample clock */
    uint32_t phase[3], noise;
    uint32_t effect_position, effect_phase;
    GdMusic music;
    GdSound effect;
    uint8_t music_level, effect_level; /* 0-100 */
} GdSynth;

void gd_synth_init(GdSynth *s);
void gd_synth_music(GdSynth *s, GdMusic music);
void gd_synth_effect(GdSynth *s, GdSound effect);
void gd_synth_levels(GdSynth *s, unsigned music_percent, unsigned effect_percent);
void gd_synth_render(GdSynth *s, int16_t *out, size_t count);
