/* Original loops and effects; square, triangle and noise voices only. */
#include "gd_synth.h"
#include <string.h>

#define STEP (GD_SYNTH_RATE / 7) /* ~143 ms per sixteenth-ish step */
/* Note numbers: 0 = rest, otherwise semitones above A2 (110 Hz). */
static const uint8_t surface_lead[32] = {20, 0, 24, 0, 27, 0, 24, 22, 20, 0, 17, 0, 20, 0, 0, 0,
                                         22, 0, 25, 0, 29, 0, 27, 25, 24, 0, 20, 0, 22, 0, 0, 0};
static const uint8_t surface_bass[8] = {8, 8, 13, 13, 10, 10, 15, 13};
static const uint8_t mine_lead[32] = {15, 0, 0, 18, 0, 0, 22, 0, 20, 0, 18, 0, 15, 0, 0, 0,
                                      13, 0, 0, 17, 0, 0, 20, 0, 18, 0, 17, 0, 13, 0, 0, 0};
static const uint8_t mine_bass[8] = {3, 3, 1, 1, 6, 6, 8, 1};

static uint32_t freq_q8(unsigned note) { /* Hz * 256 */
    static const uint16_t semis[12] = {256, 271, 287, 304, 323, 342, 362, 384, 406, 431, 456, 483};
    uint32_t f = 110u * semis[note % 12];
    return f << (note / 12);
}
static int square(uint32_t phase, unsigned duty) { return (phase >> 24) < duty ? 1 : -1; }
static int triangle(uint32_t phase) {
    int v = (int)(phase >> 23); /* 0..511 */
    return v < 256 ? v - 128 : 383 - v;
}
static uint32_t inc(uint32_t f_q8) { return (uint32_t)(((uint64_t)f_q8 << 24) / GD_SYNTH_RATE); }
static int noise(GdSynth *s) {
    uint32_t x = s->noise;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s->noise = x;
    return (int)(x & 0xff) - 128;
}

void gd_synth_init(GdSynth *s) {
    memset(s, 0, sizeof(*s));
    s->noise = 0x1234567u;
    s->music_level = 60;
    s->effect_level = 100;
}
void gd_synth_music(GdSynth *s, GdMusic m) {
    if ((unsigned)m >= GD_MUSIC_COUNT || m == s->music) return;
    s->music = m;
    s->position = 0;
}
void gd_synth_effect(GdSynth *s, GdSound e) {
    if ((unsigned)e >= GD_SOUND_COUNT || e == GD_SOUND_NONE) return;
    s->effect = e;
    s->effect_position = 0;
    s->effect_phase = 0;
}
void gd_synth_levels(GdSynth *s, unsigned music, unsigned effects) {
    s->music_level = (uint8_t)(music > 100 ? 100 : music);
    s->effect_level = (uint8_t)(effects > 100 ? 100 : effects);
}

/* One effect sample (about -128..127) or 0 when finished. */
static int effect_sample(GdSynth *s) {
    uint32_t t = s->effect_position++, ms = t * 1000u / GD_SYNTH_RATE;
    int env, v;
    switch (s->effect) {
        case GD_SOUND_MENU:
            if (ms >= 40) break;
            s->effect_phase += inc(freq_q8(39));
            return square(s->effect_phase, 128) * 50 * (40 - (int)ms) / 40;
        case GD_SOUND_DIG:
            if (ms >= 90) break;
            env = 90 - (int)ms;
            s->effect_phase += inc(freq_q8(ms < 30 ? 10 : 5));
            return (noise(s) * env / 90 + square(s->effect_phase, 128) * 40 * env / 90) / 2;
        case GD_SOUND_ORE:
        case GD_SOUND_COINS: {
            static const uint8_t ore[3] = {36, 43, 48}, coins[4] = {41, 46, 41, 53};
            unsigned notes = s->effect == GD_SOUND_ORE ? 3 : 4, each = s->effect == GD_SOUND_ORE ? 70 : 60;
            if (ms >= notes * each) break;
            unsigned n = ms / each;
            s->effect_phase += inc(freq_q8(s->effect == GD_SOUND_ORE ? ore[n] : coins[n]));
            env = (int)(each - ms % each);
            return square(s->effect_phase, 64) * 70 * env / (int)each;
        }
        case GD_SOUND_CHEST: {
            static const uint8_t notes[6] = {36, 40, 43, 48, 52, 55};
            if (ms >= 6 * 70) break;
            s->effect_phase += inc(freq_q8(notes[ms / 70]));
            return square(s->effect_phase, 96) * 70;
        }
        case GD_SOUND_GAS:
            if (ms >= 400) break;
            return noise(s) * (400 - (int)ms) / 900;
        case GD_SOUND_HURT:
            if (ms >= 350) break;
            s->effect_phase += inc(freq_q8(30 - ms / 20));
            return square(s->effect_phase, 128) * 70 * (350 - (int)ms) / 350;
        case GD_SOUND_BLAST:
            if (ms >= 700) break;
            env = 700 - (int)ms;
            s->effect_phase += inc(freq_q8(1));
            v = noise(s) * env / 700 + triangle(s->effect_phase) * env / 1400;
            return v;
        case GD_SOUND_DENY:
            if (ms >= 160) break;
            s->effect_phase += inc(freq_q8(ms < 80 ? 8 : 4));
            return square(s->effect_phase, 128) * 55;
        default: break;
    }
    s->effect = GD_SOUND_NONE;
    return 0;
}
static int music_sample(GdSynth *s) {
    uint32_t t = s->position++;
    unsigned step = (t / STEP) % 32, within = t % STEP;
    const uint8_t *lead = s->music == GD_MUSIC_MINE ? mine_lead : surface_lead;
    const uint8_t *bass = s->music == GD_MUSIC_MINE ? mine_bass : surface_bass;
    int v = 0;
    unsigned note = lead[step];
    if (note) {
        s->phase[0] += inc(freq_q8(note));
        int env = (int)(STEP - within) * 32 / (int)STEP;
        v += square(s->phase[0], 64) * env;
    }
    s->phase[1] += inc(freq_q8(bass[step / 4]));
    v += triangle(s->phase[1]) / 3;
    if (s->music == GD_MUSIC_SURFACE && (step & 3) == 0 && within < STEP / 6) v += noise(s) / 10;
    if (s->music == GD_MUSIC_MINE && (step & 7) == 4 && within < STEP / 3) {
        s->phase[2] += inc(freq_q8(51));
        v += square(s->phase[2], 128) * 10 * (int)(STEP / 3 - within) / (int)(STEP / 3);
    }
    return v;
}
void gd_synth_render(GdSynth *s, int16_t *out, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        int m = s->music_level ? music_sample(s) * s->music_level : 0;
        int e = s->effect != GD_SOUND_NONE ? effect_sample(s) * s->effect_level : 0;
        int v = (m * 3 + e * 8) / 4; /* each source peaks near +/-128*100 */
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        out[i] = (int16_t)v;
    }
}
