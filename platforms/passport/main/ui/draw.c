/* Every Gold Digger screen, drawn with LVGL primitives on one full-screen
   object in a "cozy lantern" pixel-art style: art sits on a 2x grid, sprites
   are small palette-indexed strings, and frames have notched pixel corners.
   Animation is driven by clock_ms (redrawn at 10 Hz). */
#include "ui.h"

enum {
    INK_TITLE = 0xffde9f,
    INK_BODY = 0xe4d4b2,
    INK_HINT = 0xbfcba7,
    INK_DIM = 0x8d8573,
    INK_GOOD = 0x9cbb78,
    INK_BAD = 0xe07a5f,
    PANEL = 0x101519,
    BRASS = 0x967442,
    BRASS_DIM = 0x765b37,
    ROW = 0x191d20,
    ROW_EDGE = 0x67553e,
    FOCUS = 0x492820,
    FOCUS_EDGE = 0xe0b86b,
    FOCUS_INK = 0xffdda0,
    ROW_INK = 0xc9bd9f,
};
#define TILE      30
#define MINE_TOP  25
#define MINE_ROWS 9

static lv_obj_t *screen_obj;

/* ---- hit regions ----------------------------------------------------------- */
typedef struct {
    int16_t x, y, w, h;
    int id;
} Hit;
static Hit hits[32];
static unsigned hit_count;
void hit_clear(void) { hit_count = 0; }
void hit_add(int x, int y, int w, int h, int id) {
    if (hit_count < sizeof(hits) / sizeof(hits[0])) hits[hit_count++] = (Hit){x, y, w, h, id};
}
int hit_find(int x, int y) {
    for (unsigned i = hit_count; i-- > 0;) /* later regions sit on top */
        if (x >= hits[i].x && x < hits[i].x + hits[i].w && y >= hits[i].y && y < hits[i].y + hits[i].h)
            return hits[i].id;
    return -1;
}

/* ---- primitives ------------------------------------------------------------ */
static void box(lv_layer_t *l, int x, int y, int w, int h, uint32_t fill, uint32_t edge, lv_opa_t opa, int radius) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(fill);
    d.bg_opa = opa;
    d.radius = radius;
    if (edge != fill) {
        d.border_color = lv_color_hex(edge);
        d.border_width = 1;
        d.border_opa = opa;
    }
    lv_area_t a = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(l, &d, &a);
}
static void fill(lv_layer_t *l, int x, int y, int w, int h, uint32_t color, lv_opa_t opa) {
    box(l, x, y, w, h, color, color, opa, 0);
}
static void line(lv_layer_t *l, int x1, int y1, int x2, int y2, uint32_t color, int width, lv_opa_t opa) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = lv_color_hex(color);
    d.width = width;
    d.opa = opa;
    d.round_start = d.round_end = 1;
    d.p1 = (lv_point_precise_t){x1, y1};
    d.p2 = (lv_point_precise_t){x2, y2};
    lv_draw_line(l, &d);
}
static void text_align(lv_layer_t *l, const char *s, int x, int y, int w, int h, uint32_t color,
                       const lv_font_t *font, lv_text_align_t align) {
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    s = tr(s); /* every drawn string is looked up; formatted ones were translated before snprintf */
    d.text = s;
    d.text_local = 1;
    d.font = font_for(s, font);
    d.color = lv_color_hex(color);
    d.align = align;
    d.line_space = 2;
    lv_area_t a = {x, y, x + w - 1, y + h - 1};
    lv_draw_label(l, &d, &a);
}
static void text(lv_layer_t *l, const char *s, int x, int y, int w, int h, uint32_t color, const lv_font_t *font) {
    text_align(l, s, x, y, w, h, color, font, LV_TEXT_ALIGN_LEFT);
}
/* ---- pixel art ------------------------------------------------------------- */
/* Art is drawn on a 2x grid: one art pixel = 2x2 screen pixels. Sprites are
   rows of palette indices ('1'..'9', 'a'..'f'; '.' is transparent) and are
   drawn one horizontal run at a time to keep the fill count low. */
enum { PIX = 2 };
static void sprite(lv_layer_t *l, const char *const *rows, unsigned n, int x, int y, bool flip, const uint32_t *pal) {
    for (unsigned r = 0; r < n; ++r) {
        const char *s = rows[r];
        int w = (int)strlen(s);
        for (int i = 0; i < w;) {
            char c = s[flip ? w - 1 - i : i];
            int j = i + 1;
            while (j < w && s[flip ? w - 1 - j : j] == c) ++j;
            if (c != '.') {
                int k = c <= '9' ? c - '1' : c - 'a' + 9;
                fill(l, x + i * PIX, y + (int)r * PIX, (j - i) * PIX, PIX, pal[k], LV_OPA_COVER);
            }
            i = j;
        }
    }
}
#define SPRITE(l, art, x, y, flip, pal) sprite(l, art, sizeof(art) / sizeof(art[0]), x, y, flip, pal)

/* A spinning pixel coin; size picks the art scale (>= 12 is the large one). */
static void coin(lv_layer_t *l, int x, int y, int size) {
    static const uint32_t pal[] = {0x8a5a12, 0xf2c037, 0xffde6e, 0xfff6cf};
    static const char *const face[] = {".111.", "12231", "12221", "12221", ".111."};
    static const char *const half[] = {".1.", "121", "131", "121", ".1."};
    static const char *const edge[] = {"1", "2", "2", "2", "1"};
    unsigned f = (clock_ms / 140 + (unsigned)x) % 16; /* rest on the face, then a quick spin */
    int o = (size - 10) / 2;
    if (f < 13)
        SPRITE(l, face, x + o, y + o, false, pal);
    else if (f == 14)
        SPRITE(l, edge, x + o + 4, y + o, false, pal);
    else
        SPRITE(l, half, x + o + 2, y + o, f == 15, pal);
}
/* 1,234 below 10 000; then 12.3K, 4.5M ... */
static void amount(uint64_t v, char *out, size_t cap) {
    static const char units[] = "KMBTQ";
    if (v < 10000) {
        if (v >= 1000)
            snprintf(out, cap, "%u,%03u", (unsigned)(v / 1000), (unsigned)(v % 1000));
        else
            snprintf(out, cap, "%u", (unsigned)v);
        return;
    }
    unsigned u = 0;
    uint64_t scaled = v / 100; /* tenths of thousands */
    while (scaled >= 10000 && u < sizeof(units) - 2) {
        scaled /= 1000;
        ++u;
    }
    snprintf(out, cap, "%u.%u%c", (unsigned)(scaled / 10), (unsigned)(scaled % 10), units[u]);
}
static uint32_t hash(uint32_t a, uint32_t b) {
    uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca6bu;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    return h ^ (h >> 12);
}
static uint32_t shade(uint32_t c, int percent) { /* 100 = unchanged */
    int r = (int)((c >> 16) & 255) * percent / 100, g = (int)((c >> 8) & 255) * percent / 100,
        b = (int)(c & 255) * percent / 100;
    r = r > 255 ? 255 : r;
    g = g > 255 ? 255 : g;
    b = b > 255 ? 255 : b;
    return (uint32_t)(r << 16 | g << 8 | b);
}

/* ---- chrome ----------------------------------------------------------------- */
/* Pixel frame: 2 px edge with notched corners, a 1 px inner shadow line. */
static void pbox(lv_layer_t *l, int x, int y, int w, int h, uint32_t body, uint32_t edge, lv_opa_t opa) {
    fill(l, x + 2, y, w - 4, 2, edge, opa);
    fill(l, x + 2, y + h - 2, w - 4, 2, edge, opa);
    fill(l, x, y + 2, 2, h - 4, edge, opa);
    fill(l, x + w - 2, y + 2, 2, h - 4, edge, opa);
    fill(l, x + 2, y + 2, w - 4, h - 4, body, opa);
    fill(l, x + 2, y + 2, w - 4, 1, shade(edge, 140), opa); /* top glint */
    fill(l, x + 2, y + h - 3, w - 4, 1, shade(body, 60), opa);
}
static void header(lv_layer_t *l, const char *title, bool gold) {
    pbox(l, 2, 2, 236, 24, PANEL, BRASS, 245);
    int x = 9;
    if (pointer_mode && page != HOME && page != MINE && page != LANGUAGE_PICK) {
        static const uint32_t pal[] = {FOCUS_INK};
        static const char *const arrow[] = {"..1", ".11", "111", ".11", "..1"};
        pbox(l, 5, 5, 22, 18, ROW, ROW_EDGE, LV_OPA_COVER);
        SPRITE(l, arrow, 13, 9, false, pal);
        hit_add(0, 0, 40, 30, HIT_BACK);
        x = 32;
    }
    if (gold) {
        char g[24];
        amount(game.profile.gold, g, sizeof(g));
        coin(l, x, 9, 10);
        text(l, g, x + 14, 7, 80, 16, INK_TITLE, &lv_font_montserrat_12);
    }
    text_align(l, title, 100, 7, gold ? 100 : 130, 16, INK_BODY, &lv_font_montserrat_12,
               gold ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_LEFT);
    if (!pointer_mode && battery_percent >= 0) {
        int p = battery_percent > 100 ? 100 : battery_percent;
        pbox(l, 210, 8, 20, 12, 0x18201c, 0xb8be9e, LV_OPA_COVER);
        fill(l, 230, 12, 2, 4, 0xb8be9e, LV_OPA_COVER);
        int bars = (p + 24) / 25; /* chunky four-cell meter */
        uint32_t c = battery_charging ? 0xffe16b : p < 20 ? 0xcc7959 : 0x9cbb78;
        for (int i = 0; i < bars && i < 4; ++i) fill(l, 213 + i * 4, 11, 3, 6, c, LV_OPA_COVER);
    }
}
static void footer(lv_layer_t *l, const char *hint) {
    pbox(l, 2, 295, 236, 23, PANEL, BRASS_DIM, 245);
    bool fresh = notice[0] && clock_ms < notice_until;
    text_align(l, fresh ? notice : hint, 8, 300, 224, 14, fresh ? INK_TITLE : INK_HINT, &lv_font_montserrat_10,
               LV_TEXT_ALIGN_CENTER);
}
/* Underground backdrop for menus: blocky strata, brick seams and twinkling ore. */
static void backdrop(lv_layer_t *l) {
    for (int y = 0; y < 320; y += 16) {
        GdTile t = gd_stratum((unsigned)(y * GD_ROWS / 320));
        uint32_t c = shade(gd_tile_info(t)->color, 30 + (y / 16 % 2) * 4);
        fill(l, 0, y, 240, 16, c, LV_OPA_COVER);
        fill(l, 0, y + 14, 240, 2, shade(c, 70), LV_OPA_COVER);
        for (int x = (y / 16 % 2) * 16; x < 240; x += 32) fill(l, x, y, 2, 14, shade(c, 70), LV_OPA_COVER);
    }
    for (unsigned i = 0; i < 26; ++i) {
        uint32_t h = hash(i, 77);
        int x = (int)(h % 236) & ~1, y = (int)((h >> 8) % 316) & ~1;
        GdTile ore = (GdTile)(GD_COPPER + (h >> 20) % 5);
        uint32_t c = gd_tile_info(ore)->color;
        bool glint = (clock_ms / 100 + h) % 40 == 0;
        fill(l, x, y, 4, 2, shade(c, glint ? 140 : 55), LV_OPA_COVER);
        if (glint) {
            fill(l, x + 1, y - 2, 2, 6, 0xfff6cf, LV_OPA_COVER);
            fill(l, x - 1, y, 6, 2, 0xfff6cf, LV_OPA_COVER);
        }
    }
}
/* A bobbing pickaxe pointer for the focused row. */
static void cursor(lv_layer_t *l, int x, int y) {
    static const uint32_t pal[] = {0xb9c2c8, 0x8a5a32, 0x6a7276};
    static const char *const pick[] = {".11..", "1.21.", "..2.3", ".2...", "2...."};
    int bob = (clock_ms / 200) % 2;
    SPRITE(l, pick, x + bob * 2, y, false, pal);
}
/* A list of rows; returns the bottom edge. */
static int rows(lv_layer_t *l, int y0, unsigned count, unsigned visible, char labels[][40], char values[][24],
                const uint32_t *value_ink) {
    unsigned first = 0;
    if (count > visible && selected >= visible / 2) first = selected - visible / 2;
    if (first + visible > count) first = count > visible ? count - visible : 0;
    int y = y0;
    for (unsigned i = first; i < count && i < first + visible; ++i, y += 30) {
        bool focus = i == selected;
        pbox(l, 10, y, 220, 26, focus ? FOCUS : ROW, focus ? FOCUS_EDGE : ROW_EDGE, 245);
        if (focus) cursor(l, 14, y + 8);
        text(l, labels[i], focus ? 30 : 18, y + 6, 130, 16, focus ? FOCUS_INK : ROW_INK, &lv_font_montserrat_12);
        if (values && values[i][0])
            text_align(l, values[i], 120, y + 6, 104, 16, value_ink ? value_ink[i] : INK_TITLE,
                       &lv_font_montserrat_12, LV_TEXT_ALIGN_RIGHT);
        hit_add(10, y, 220, 26, HIT_ROW + (int)i);
    }
    if (first) text_align(l, LV_SYMBOL_UP, 220, y0 - 10, 16, 10, INK_DIM, &lv_font_montserrat_8, LV_TEXT_ALIGN_CENTER);
    if (first + visible < count)
        text_align(l, LV_SYMBOL_DOWN, 220, y, 16, 10, INK_DIM, &lv_font_montserrat_8, LV_TEXT_ALIGN_CENTER);
    return y;
}
static void panel(lv_layer_t *l, int x, int y, int w, int h) { pbox(l, x, y, w, h, PANEL, BRASS_DIM, 240); }

/* ---- the miner -------------------------------------------------------------- */
/* 11x12 art, facing right. 1 hat, 2 brim, 3 lamp, 4 skin, 5 eye, 6 cheek,
   7 shirt, 8 overalls, 9 overall shade, a boots. */
static const uint32_t miner_pal[] = {0xf2c037, 0xb07a1c, 0xfff6c8, 0xf2c09a, 0x2a1a12,
                                     0xe8806a, 0xc94b3c, 0x3a5a8a, 0x23395a, 0x3a2a1c};
static const char *const miner_body[] = {
    "...11111...", "..11111113.", ".222222222.", "..4444444..", "..4444545..", "..6444446..",
    "...44444...", "..7787787..", ".47888887..", "..8888888..", "..89...98..", "..aa...aa.."};
static const char *const miner_blink[] = {"..4444444.."};
static const char *const miner_step[] = {"...89.98...", "...aa.aa..."};
/* pickaxes: 1 steel, 2 handle */
static const uint32_t pick_pal[] = {0xb9c2c8, 0x8a5a32};
static const char *const pick_rest[] = {".111.", "1.2.1", "..2..", "..2..", "..2.."};
static const char *const pick_side[] = {"....1", "....1", "22221", "....1", "....1"};
static const char *const pick_down[] = {"..2..", "..2..", "..2..", "11111", "1...1"};
/* (x, y) = top-left of a 30x30 cell. pose: 0 idle, 1 step, 2 strike sideways, 3 strike down. */
static void miner(lv_layer_t *l, int x, int y, bool right, int pose) {
    int bob = pose == 0 && (clock_ms / 400) % 2 ? 2 : 0;
    int ox = x + 4, oy = y + 6 + bob;
    /* soft lamp beam: three stepped bands that flicker slightly */
    int flick = (clock_ms / 120) % 5 == 0;
    for (int i = 0; i < 3; ++i) {
        int bx = right ? ox + 20 + i * 8 : ox - 6 - i * 8 - 8;
        fill(l, bx, oy + 2 - i * 2, 8, 4 + i * 4, 0xfff2b0, (lv_opa_t)(70 - i * 20 - flick * 10));
    }
    sprite(l, miner_body, 10, ox, oy, !right, miner_pal);
    sprite(l, pose == 1 ? miner_step : miner_body + 10, 2, ox, oy + 20, !right, miner_pal);
    if ((clock_ms / 100) % 32 == 0) SPRITE(l, miner_blink, ox, oy + 8, !right, miner_pal);
    if (pose == 2)
        SPRITE(l, pick_side, right ? ox + 16 : ox - 4, oy + 12, !right, pick_pal);
    else if (pose == 3)
        SPRITE(l, pick_down, right ? ox + 12 : ox, oy + 16, false, pick_pal);
    else
        SPRITE(l, pick_rest, right ? ox + 14 : ox - 2, oy + 4, false, pick_pal);
}

/* ---- surface ---------------------------------------------------------------- */
static const int station_x[STATION_COUNT] = {38, 104, 164, 214};
static const char *const station_name[STATION_COUNT] = {"Trading Post", "Mine Shaft", "Elevator", "Settings"};
static void draw_home(lv_layer_t *l) {
    static const uint32_t sky[7] = {0x141a2c, 0x1a2136, 0x222840, 0x2c2e48, 0x3a344e, 0x4a3c50, 0x5a4452};
    for (int i = 0; i < 7; ++i) fill(l, 0, 26 + i * 20, 240, 20, sky[i], LV_OPA_COVER);
    for (int i = 0; i < 6; ++i) /* dithered seams between sky bands */
        for (int x = (i & 1) * 4; x < 240; x += 8) fill(l, x, 44 + i * 20, 2, 2, sky[i + 1], LV_OPA_COVER);
    for (unsigned i = 0; i < 22; ++i) {
        uint32_t h = hash(i, 5);
        int x = (int)(h % 240) & ~1, y = 30 + ((int)((h >> 9) % 90) & ~1);
        unsigned ph = (clock_ms / 150 + i * 5) % 24;
        uint32_t c = ph < 2 ? 0xffffff : 0xfff6cf;
        fill(l, x, y, 2, 2, c, ph < 20 ? 220 : 70);
        if (ph < 2) { /* a twinkle cross */
            fill(l, x - 2, y, 6, 2, c, 160);
            fill(l, x, y - 2, 2, 6, c, 160);
        }
    }
    { /* pixel moon */
        static const uint32_t pal[] = {0xf4e6c0, 0xd8c79c};
        static const char *const moon[] = {"..111..", ".11....", "112....", "11.....", "112....", ".11....", "..111.."};
        SPRITE(l, moon, 194, 40, false, pal);
    }
    { /* a cloud drifting across */
        int cx = (int)((clock_ms / 120) % 300) - 40;
        cx &= ~1;
        fill(l, cx, 70, 36, 6, 0x4a4a62, 200);
        fill(l, cx + 8, 66, 18, 4, 0x4a4a62, 200);
    }
    /* stepped hills */
    for (int i = 0; i < 8; ++i) {
        fill(l, 0, 138 + i * 4, 40 + i * 12, 4, 0x232a30, LV_OPA_COVER);
        fill(l, 230 - 40 - i * 14, 130 + i * 5, 50 + i * 14 + 10, 5, 0x1d2328, LV_OPA_COVER);
    }
    /* ground cross-section: grass lip, tufts, then blocky strata */
    fill(l, 0, 166, 240, 4, 0x6b9a44, LV_OPA_COVER);
    fill(l, 0, 170, 240, 2, 0x4a6e34, LV_OPA_COVER);
    for (int x = 2; x < 240; x += 14) fill(l, x + (x * 7 % 4), 164, 2, 2, 0x6b9a44, LV_OPA_COVER);
    for (int y = 172, band = 0; y < 296; y += 14, ++band) {
        GdTile t = gd_stratum((unsigned)band * 40);
        uint32_t c = shade(gd_tile_info(t)->color, 70);
        fill(l, 0, y, 240, 14, c, LV_OPA_COVER);
        fill(l, 0, y + 12, 240, 2, shade(c, 75), LV_OPA_COVER);
        for (int x = (band & 1) * 10; x < 240; x += 20) fill(l, x, y + 4, 4, 2, shade(c, 120), LV_OPA_COVER);
    }
    for (unsigned i = 0; i < 18; ++i) {
        uint32_t h = hash(i, 9);
        GdTile ore = (GdTile)(GD_COAL + (h >> 24) % 6);
        uint32_t c = gd_tile_info(ore)->color;
        int x = (int)(h % 232) & ~1, y = (176 + (int)((h >> 8) % 112)) & ~1;
        fill(l, x, y, 4, 4, c, LV_OPA_COVER);
        fill(l, x, y, 2, 2, shade(c, 150), LV_OPA_COVER);
        if ((clock_ms / 100 + h) % 30 == 0) fill(l, x + 4, y - 2, 2, 2, 0xffffff, LV_OPA_COVER);
    }
    fill(l, 96, 166, 16, 130, 0x0b0d0e, LV_OPA_COVER); /* shaft */
    fill(l, 94, 166, 2, 130, 0x3a2a1c, LV_OPA_COVER);
    fill(l, 112, 166, 2, 130, 0x3a2a1c, LV_OPA_COVER);
    { /* trading post: a log cabin with a glowing window and chimney smoke */
        static const uint32_t pal[] = {0x7a3a2a, 0x5a2a1e, 0x8a5a32, 0x6a4229, 0x3a2418, 0xffd27a, 0xe0a050, 0x9a9a9a};
        static const char *const cabin[] = {
            "..........88.......", "..........88.......", "........11111......", ".......1111111.....",
            "......112222211....", ".....11222222211...", "....1111111111111..", "...111111111111111.",
            "....3434343434343..", "....4343434343434..", "....3466343434343..", "....4677434343434..",
            "....3466343455343..", "....4343434355434..", "....3434343455343..", "....4343434355434..",
            "....3434343455343.."};
        SPRITE(l, cabin, 4, 134, false, pal);
        if ((clock_ms / 300) % 9 == 0) fill(l, 18, 156, 8, 6, 0xffeaa0, 120); /* window flicker */
        for (unsigned i = 0; i < 3; ++i) { /* smoke puffs rise and fade */
            unsigned t = (clock_ms / 80 + i * 12) % 36;
            int sx = 24 + (int)(t / 6) % 2 * 2, sy = 134 - (int)t * 2;
            fill(l, sx & ~1, sy & ~1, 4 + (int)t / 12 * 2, 4, 0x8a8a96, (lv_opa_t)(180 - t * 4));
        }
        coin(l, 36, 124, 12);
    }
    { /* headframe: stepped A-frame with a turning wheel */
        for (int i = 0; i < 14; ++i) {
            fill(l, 88 + i, 166 - i * 4 - 4, 2, 4, 0x8a6a42, LV_OPA_COVER);
            fill(l, 118 - i, 166 - i * 4 - 4, 2, 4, 0x8a6a42, LV_OPA_COVER);
        }
        fill(l, 92, 148, 24, 2, 0x8a6a42, LV_OPA_COVER);
        fill(l, 96, 130, 16, 2, 0x8a6a42, LV_OPA_COVER);
        static const uint32_t pal[] = {0xb9a27a, 0x2b2b2b};
        static const char *const w0[] = {"..111..", ".1.1.1.", "1..1..1", "1111111", "1..1..1", ".1.1.1.", "..111.."};
        static const char *const w1[] = {"..111..", ".11..1.", "1.1.1.1", "1..1..1", "1.1.1.1", ".1...1.", "..111.."};
        if ((clock_ms / 150) % 2)
            SPRITE(l, w1, 97, 104, false, pal);
        else
            SPRITE(l, w0, 97, 104, false, pal);
        fill(l, 104, 118, 1, 172, 0xb9a27a, LV_OPA_COVER);
    }
    { /* elevator tower with a bobbing cage */
        fill(l, 150, 100, 2, 68, 0x6d7a80, LV_OPA_COVER);
        fill(l, 176, 100, 2, 68, 0x6d7a80, LV_OPA_COVER);
        for (int y = 108; y < 166; y += 12) fill(l, 152, y, 24, 2, 0x4d5a60, LV_OPA_COVER);
        fill(l, 148, 98, 32, 4, 0x6d7a80, LV_OPA_COVER);
        int bob = (int)((clock_ms / 90) % 40);
        int cy = (112 + (bob < 20 ? bob : 40 - bob)) & ~1;
        fill(l, 164, 102, 1, cy - 102, 0xb9c2c8, LV_OPA_COVER);
        pbox(l, 156, cy, 16, 18, 0x2e3a3f, 0x9aa8ae, LV_OPA_COVER);
        fill(l, 160, cy + 4, 8, 6, 0xffd27a, (clock_ms / 500) % 2 ? 200 : 140);
    }
    { /* settings signpost with a pixel gear */
        fill(l, 212, 132, 4, 36, 0x6a4a2a, LV_OPA_COVER);
        pbox(l, 196, 122, 36, 18, 0x8a6a42, 0x5a3a1a, LV_OPA_COVER);
        static const uint32_t pal[] = {0x2a1a0a};
        static const char *const g0[] = {"1.1.1", ".111.", "11.11", ".111.", "1.1.1"};
        static const char *const g1[] = {"..1..", "11111", ".1.1.", "11111", "..1.."};
        if ((clock_ms / 250) % 2)
            SPRITE(l, g1, 209, 126, false, pal);
        else
            SPRITE(l, g0, 209, 126, false, pal);
    }
    /* stations, selection and the miner */
    for (unsigned s = 0; s < STATION_COUNT; ++s) hit_add(station_x[s] - 26, 90, 52, 82, HIT_ROW + (int)s);
    int sx = station_x[selected];
    {
        static const uint32_t pal[] = {INK_TITLE, 0x8a5a12};
        static const char *const arrow[] = {"11111", "21112", ".111.", ".212.", "..1.."};
        int bounce = (int)((clock_ms / 150) % 4);
        SPRITE(l, arrow, sx - 5, 84 + (bounce < 2 ? bounce : 4 - bounce) * 2, false, pal);
    }
    { /* the miner paces a few steps beside the chosen station */
        bool left = selected == STATION_SETTINGS;
        unsigned t = (clock_ms / 100) % 40;
        int walk = t < 10 ? (int)t : t < 20 ? 10 : t < 30 ? 30 - (int)t : 0;
        bool moving = (t < 10 || (t >= 20 && t < 30));
        bool right = left ? t >= 20 : t < 20;
        int base = sx - 15 + (left ? -14 : 14);
        miner(l, base + (left ? -walk : walk) * 1, 139, right, moving && (clock_ms / 150) % 2 ? 1 : 0);
    }
    /* info card */
    panel(l, 8, 222, 224, 70);
    char line1[64], line2[64], best[16];
    text(l, station_name[selected], 16, 228, 208, 18, INK_TITLE, &lv_font_montserrat_14);
    switch (selected) {
        case STATION_MINE:
            snprintf(line1, sizeof(line1), "%s", game.run.active ? "Resume your dive" : "Dive from the surface");
            snprintf(line2, sizeof(line2), tr("Best depth %u m  -  Dives %lu"), game.profile.best_depth,
                     (unsigned long)game.profile.dives);
            break;
        case STATION_SHOP:
            snprintf(line1, sizeof(line1), "Upgrade pickaxe, lantern, bag");
            snprintf(line2, sizeof(line2), tr("Pickaxe Lv %u  Lantern Lv %u  Bag Lv %u"), game.profile.level[GD_PICK] + 1,
                     game.profile.level[GD_LANTERN] + 1, game.profile.level[GD_BAG] + 1);
            break;
        case STATION_ELEVATOR: {
            unsigned owned = 0;
            for (unsigned s = 1; s < GD_STOP_COUNT; ++s) owned += gd_stop_owned(&game.profile, s);
            snprintf(line1, sizeof(line1), tr("Start deeper: a stop every %u m"), GD_STOP_SPACING);
            snprintf(line2, sizeof(line2), tr("%u of %u stops open"), owned, GD_STOP_COUNT - 1);
            break;
        }
        default:
            amount(game.profile.lifetime_gold, best, sizeof(best));
            snprintf(line1, sizeof(line1), "Sound, language, sleep");
            snprintf(line2, sizeof(line2), tr("Lifetime gold %s"), best);
            break;
    }
    text(l, line1, 16, 248, 208, 16, INK_BODY, &lv_font_montserrat_12);
    text(l, line2, 16, 268, 208, 16, INK_DIM, &lv_font_montserrat_10);
    header(l, "Gold Digger", true);
    footer(l, pointer_mode ? "Tap a place to go there" : "UP/DOWN choose   OK enter");
}

/* ---- the mine ----------------------------------------------------------------- */
/* 4x4 art nugget: base colour with a highlight and a shadow. */
static void nuggets(lv_layer_t *l, int x, int y, uint32_t seed, uint32_t color, unsigned count, bool sparkle) {
    uint32_t pal[] = {color, shade(color, 160), shade(color, 55)};
    static const char *const art[] = {".11.", "1211", "1113", ".33."};
    for (unsigned i = 0; i < count; ++i) {
        uint32_t h = hash(seed, i + 11);
        int nx = x + 4 + ((int)(h % 14) & ~1), ny = y + 4 + ((int)((h >> 10) % 14) & ~1);
        SPRITE(l, art, nx, ny, false, pal);
        if (sparkle && (clock_ms / 100 + h) % 18 == 0) {
            fill(l, nx + 2, ny - 2, 2, 6, 0xffffff, LV_OPA_COVER);
            fill(l, nx, ny, 6, 2, 0xffffff, LV_OPA_COVER);
        }
    }
}
static void draw_tile(lv_layer_t *l, GdTile t, int px, int py, int x, int y) {
    uint32_t seed = hash((uint32_t)x, (uint32_t)y);
    uint32_t rock = gd_tile_info(gd_stratum((unsigned)y))->color;
    if (t == GD_AIR) {
        fill(l, px, py, TILE, TILE, y == 0 ? 0x2c2e48 : shade(rock, 22), LV_OPA_COVER);
        if (y) {
            fill(l, px + ((int)(seed % 20) & ~1), py + ((int)((seed >> 8) % 24) & ~1), 4, 2, shade(rock, 34), LV_OPA_COVER);
            fill(l, px, py + TILE - 2, TILE, 2, shade(rock, 16), LV_OPA_COVER);
        }
        return;
    }
    if (t == GD_LAVA) {
        fill(l, px, py, TILE, TILE, 0xd8401a, LV_OPA_COVER);
        fill(l, px, py + 10, TILE, 10, 0xb83010, LV_OPA_COVER);
        for (unsigned i = 0; i < 3; ++i) {
            uint32_t h = hash(seed, i);
            int bx = px + 2 + ((int)(h % 22) & ~1), by = py + 4 + ((int)((h >> 8) + clock_ms / 80) % 22 & ~1);
            fill(l, bx, by, 4, 4, 0xffb02e, LV_OPA_COVER);
            fill(l, bx, by, 2, 2, 0xffe08a, LV_OPA_COVER);
        }
        int wave = (int)(clock_ms / 150) % 4 * 2;
        for (int k = -8; k < TILE; k += 8) fill(l, px + ((k + wave) < 0 ? 0 : k + wave), py, 4, 2, 0xffd34a, LV_OPA_COVER);
        fill(l, px, py + 2, TILE, 2, 0xffa030, LV_OPA_COVER);
        return;
    }
    if (t == GD_BEDROCK) {
        fill(l, px, py, TILE, TILE, 0x16161a, LV_OPA_COVER);
        for (int k = 0; k < TILE; k += 6) fill(l, px + k, py + (k % 12 ? 4 : 16), 4, 4, 0x2a2a33, LV_OPA_COVER);
        return;
    }
    GdTile base = t <= GD_OBSIDIAN ? t : gd_stratum((unsigned)y);
    rock = gd_tile_info(base)->color;
    /* bevelled block: light top/left edge, dark bottom/right edge */
    fill(l, px, py, TILE, TILE, rock, LV_OPA_COVER);
    fill(l, px, py, TILE, 2, shade(rock, 125), LV_OPA_COVER);
    fill(l, px, py, 2, TILE, shade(rock, 112), LV_OPA_COVER);
    fill(l, px, py + TILE - 2, TILE, 2, shade(rock, 62), LV_OPA_COVER);
    fill(l, px + TILE - 2, py, 2, TILE, shade(rock, 70), LV_OPA_COVER);
    for (unsigned i = 0; i < 3; ++i) {
        uint32_t h = hash(seed, i + 3);
        int sx = px + 4 + ((int)(h % 20) & ~1), sy = py + 4 + ((int)((h >> 8) % 20) & ~1);
        fill(l, sx, sy, 4, 2, shade(rock, 75), LV_OPA_COVER);
        fill(l, sx, sy - 2, 2, 2, shade(rock, 118), LV_OPA_COVER);
    }
    switch (t) {
        case GD_COAL: nuggets(l, px, py, seed, 0x2a2626, 3, false); break;
        case GD_COPPER: nuggets(l, px, py, seed, 0xd27a3c, 3, false); break;
        case GD_SILVER: nuggets(l, px, py, seed, 0xd8dde2, 3, true); break;
        case GD_GOLD: nuggets(l, px, py, seed, 0xf2c037, 3, true); break;
        case GD_RUBY: nuggets(l, px, py, seed, 0xe0344e, 2, true); break;
        case GD_DIAMOND: nuggets(l, px, py, seed, 0x7fe8ff, 2, true); break;
        case GD_CHEST: {
            static const uint32_t pal[] = {0x4a2a10, 0x8a5a22, 0xf2c037, 0xffde6e};
            static const char *const chest[] = {".11111111.", "1222222221", "1222222221", "3333443333",
                                                "1222442221", "1222222221", "1111111111"};
            int bob = (clock_ms / 300) % 2 * 2;
            SPRITE(l, chest, px + 5, py + 8 - bob + 2, false, pal);
            if ((clock_ms / 100 + seed) % 20 == 0) fill(l, px + 22, py + 6, 2, 2, 0xffffff, LV_OPA_COVER);
            break;
        }
        case GD_GAS:
            if (gd_sees_gas(&game.profile))
                for (unsigned i = 0; i < 3; ++i) {
                    uint32_t h = hash(seed, i + 40);
                    int drift = (int)((clock_ms / 200 + h) % 6) - 3;
                    int gx = px + 4 + ((int)(h % 16) & ~1), gy = py + 4 + (((int)((h >> 8) % 16) + drift) & ~1);
                    fill(l, gx, gy, 6, 4, 0x9fd36a, 150);
                    fill(l, gx + 2, gy - 2, 2, 2, 0x9fd36a, 150);
                }
            break;
        default: break;
    }
}
static void draw_mine(lv_layer_t *l) {
    const GdRun *r = &game.run;
    int top = (int)r->y - 3;
    if (top < 0) top = 0;
    if (top > GD_ROWS - MINE_ROWS) top = GD_ROWS - MINE_ROWS;
    int jx = clock_ms < shake_until ? ((int)(hash(clock_ms, 1) % 3) - 1) * 2 : 0;
    unsigned radius = gd_light_radius(&game.profile);
    bool flicker = (clock_ms / 130) % 7 == 0;
    fill(l, 0, 0, 240, 320, 0x0b0d0e, LV_OPA_COVER);
    for (int row = 0; row < MINE_ROWS; ++row) {
        int y = top + row, py = MINE_TOP + row * TILE;
        for (int x = 0; x < GD_COLS; ++x) {
            int px = x * TILE + jx;
            draw_tile(l, gd_tile(r, x, y), px, py, x, y);
            int dx = x - r->x, dy = y - r->y;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            int d = dx > dy ? dx : dy;
            if (y > 0 && d > (int)radius) {
                if (d > (int)radius + 1)
                    fill(l, px, py, TILE, TILE, 0x000000, 215);
                else { /* dithered rim of the lamp light */
                    fill(l, px, py, TILE, TILE, 0x000000, flicker ? 170 : 140);
                    for (int k = 0; k < TILE; k += 6) fill(l, px + k, py + (k / 6 % 2) * 14, 4, 4, 0x000000, 90);
                }
            }
            /* The tile being picked at shows pixel cracks, more with each hit. */
            if (r->crack_hits && r->crack_x == x && r->crack_y == y) {
                static const int8_t seg[][2] = {{12, 2}, {10, 6}, {12, 10}, {16, 12}, {8, 12}, {6, 16}, {18, 16}, {20, 6}};
                unsigned n = r->crack_hits * 2;
                for (unsigned i = 0; i < n && i < 8; ++i)
                    fill(l, px + seg[i][0], py + seg[i][1], 2, 4, 0x111111, LV_OPA_COVER);
            }
            /* Neighbours that can never be dug get a red pixel cross. */
            if (d == 1 && (dx == 0 || dy == 0) && y >= r->y) {
                GdTile t = gd_tile(r, x, y);
                if (t == GD_LAVA || t == GD_BEDROCK) {
                    static const uint32_t pal[] = {0xe07a5f};
                    static const char *const cross[] = {"1...1", ".1.1.", "..1..", ".1.1.", "1...1"};
                    SPRITE(l, cross, px + 2, py + 2, false, pal);
                }
            }
        }
        if (y > 0 && y % 10 == 0) {
            char m[8];
            snprintf(m, sizeof(m), "%dm", y);
            text(l, m, 2, py + 1, 30, 10, INK_HINT, &lv_font_montserrat_8);
        }
        if (y == 0) { /* grass lip */
            fill(l, 0, py + TILE - 4, 240, 2, 0x6b9a44, LV_OPA_COVER);
            fill(l, 0, py + TILE - 2, 240, 2, 0x4a6e34, LV_OPA_COVER);
        }
    }
    int mx = r->x * TILE + jx, my = MINE_TOP + ((int)r->y - top) * TILE;
    for (int y = MINE_TOP; y < my + 2; y += 4) fill(l, mx + 15, y, 1, 2, 0xb9a27a, 140); /* rope */
    uint32_t since = swing_at ? clock_ms - swing_at : 1000;
    int pose = since < 160 ? (swing_dir == GD_DOWN ? 3 : 2) : 0;
    miner(l, mx, my, r->facing, pose);
    if (since < 420) { /* rock chips fly from the struck face and fall */
        int cx = swing_dir == GD_DOWN ? mx + 15 : r->facing ? mx + 30 : mx;
        int cy = swing_dir == GD_DOWN ? my + 28 : my + 16;
        uint32_t c = gd_tile_info(gd_stratum(r->y + (swing_dir == GD_DOWN)))->color;
        int t = (int)since / 30;
        for (unsigned i = 0; i < 5; ++i) {
            uint32_t h = hash(swing_at, i);
            int vx = (int)(h % 7) - 3, vy = -2 - (int)((h >> 8) % 3);
            if (swing_dir != GD_DOWN) vx = r->facing ? -1 - (int)(h % 3) : 1 + (int)(h % 3);
            int qx = cx + vx * t, qy = cy + vy * t + t * t / 2;
            fill(l, qx & ~1, qy & ~1, 2, 2, i & 1 ? shade(c, 130) : c, LV_OPA_COVER);
        }
    }
    for (unsigned i = 0; i < FLOATERS; ++i) {
        Floater *f = &floaters[i];
        if (!f->start || clock_ms - f->start > 900) continue;
        int k = (int)(clock_ms - f->start) / 30; /* 0..30: pop up fast, then drift */
        int rise = k < 6 ? k * 4 : k < 9 ? 24 - (k - 6) * 2 : 18 + (k - 9) / 2;
        int fx = f->x * TILE + 15 - 40, fy = (MINE_TOP + ((int)f->row - top) * TILE - 6 - rise) & ~1;
        uint32_t pal[] = {f->color, shade(f->color, 160), shade(f->color, 55)};
        static const char *const gem[] = {".11.", "1211", "1113", ".33."};
        SPRITE(l, gem, fx + 4, fy + 3, false, pal);
        text_align(l, f->text, fx + 9, fy + 1, 80, 14, 0x000000, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
        text_align(l, f->text, fx + 8, fy, 80, 14, f->color, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
    }
    /* HUD */
    pbox(l, 2, 2, 236, 23, PANEL, BRASS, 245);
    char s[32];
    snprintf(s, sizeof(s), tr("%u m"), r->y);
    text(l, s, 9, 7, 50, 14, INK_TITLE, &lv_font_montserrat_12);
    int cells = r->energy_max ? (19 * r->energy + r->energy_max - 1) / r->energy_max : 0; /* segmented bar */
    pbox(l, 60, 7, 82, 14, 0x18201c, 0xb8be9e, LV_OPA_COVER);
    uint32_t ec = r->energy * 5 <= r->energy_max ? 0xcc5a4a : r->energy * 2 <= r->energy_max ? 0xe0b04a : 0x9cbb78;
    bool low = r->energy * 5 <= r->energy_max && (clock_ms / 250) % 2;
    for (int i = 0; i < cells; ++i) fill(l, 63 + i * 4, 10, 3, 8, low ? shade(ec, 70) : ec, 150);
    snprintf(s, sizeof(s), "%u/%u", r->energy, r->energy_max); /* one swing = one energy */
    text_align(l, s, 61, 8, 82, 12, 0x000000, &lv_font_montserrat_10, LV_TEXT_ALIGN_CENTER);
    text_align(l, s, 60, 7, 82, 12, 0xffffff, &lv_font_montserrat_10, LV_TEXT_ALIGN_CENTER);
    snprintf(s, sizeof(s), "%u/%u", gd_bag_used(r), r->bag_cap);
    {
        static const uint32_t pal[] = {0x5a3a1a, 0x8a6a42, 0xc9a06a};
        static const char *const bag[] = {"..1..", ".1.1.", "12221", "12321", "12221", ".111."};
        SPRITE(l, bag, 150, 8, false, pal);
    }
    text(l, s, 163, 7, 44, 14, gd_bag_used(r) >= r->bag_cap ? INK_BAD : INK_BODY, &lv_font_montserrat_12);
    snprintf(s, sizeof(s), "x%u", game.profile.level[GD_DYNAMITE]);
    {
        static const uint32_t pal[] = {0xc94b3c, 0xffd34a, 0x8e3028};
        static const char *const tnt[] = {".2", "2.", "1.", "3.", "1.", "3."};
        SPRITE(l, tnt, 206, 8, false, pal);
        if ((clock_ms / 120) % 2) fill(l, 208, 6, 2, 2, 0xffd34a, LV_OPA_COVER);
    }
    text(l, s, 214, 7, 22, 14, INK_BODY, &lv_font_montserrat_10);
    if (pointer_mode) {
        /* Tap left, right or below the miner; Menu opens the pause card. */
        hit_add(0, MINE_TOP, 80, MINE_ROWS * TILE, HIT_LEFT);
        hit_add(160, MINE_TOP, 80, MINE_ROWS * TILE, HIT_RIGHT);
        hit_add(80, MINE_TOP, 80, MINE_ROWS * TILE, HIT_DIG);
        hit_add(mx - TILE, my, TILE, TILE, HIT_LEFT);
        hit_add(mx + TILE, my, TILE, TILE, HIT_RIGHT);
        hit_add(mx, my + TILE, TILE, TILE * 2, HIT_DIG);
        footer(l, "Tap left / below / right      ");
        pbox(l, 186, 298, 48, 17, FOCUS, FOCUS_EDGE, LV_OPA_COVER);
        text_align(l, "Menu", 186, 300, 48, 14, FOCUS_INK, &lv_font_montserrat_10, LV_TEXT_ALIGN_CENTER);
        hit_add(180, 292, 60, 28, HIT_MENU);
    } else
        footer(l, "UP left  DOWN right  OK dig  hold OK menu");
}
static void draw_pause(lv_layer_t *l) {
    draw_mine(l);
    hit_clear();
    fill(l, 0, 0, 240, 320, 0x000000, 150);
    pbox(l, 4, 70, 232, 176, PANEL, FOCUS_EDGE, 250);
    text(l, "Paused", 28, 80, 180, 20, INK_TITLE, &lv_font_montserrat_14);
    char bag[16], labels[PAUSE_ROWS][40], values[PAUSE_ROWS][24] = {{0}};
    amount(gd_bag_value(&game), bag, sizeof(bag));
    snprintf(labels[0], 40, "Keep digging");
    snprintf(labels[1], 40, "Light dynamite");
    snprintf(values[1], 24, "x%u", game.profile.level[GD_DYNAMITE]);
    snprintf(labels[2], 40, "Rope up and sell");
    snprintf(values[2], 24, tr("%s gold"), bag);
    rows(l, 106, PAUSE_ROWS, PAUSE_ROWS, labels, values, NULL);
    text(l, "Fainting loses half the bag; lava loses all of it.", 28, 200, 184, 40, INK_DIM, &lv_font_montserrat_10);
    footer(l, pointer_mode ? "Tap an option" : "UP/DOWN choose   OK confirm");
}

/* ---- menus -------------------------------------------------------------------- */
static void draw_report(lv_layer_t *l) {
    const GdReport *rep = &game.report;
    backdrop(l);
    header(l, "Dive report", true);
    panel(l, 10, 32, 220, 258);
    static const char *const titles[] = {"Back on the surface", "Hauled to the surface", "Fainted! Rescued",
                                         "Lost in the lava!"};
    static const uint32_t inks[] = {INK_TITLE, INK_TITLE, 0xf0a860, INK_BAD};
    text(l, titles[rep->end], 20, 40, 200, 20, inks[rep->end], &lv_font_montserrat_14);
    int y = 64;
    char s[48], v[24];
    bool any_ore = false;
    for (unsigned i = 0; i < GD_ORE_KINDS; ++i) any_ore |= rep->ore[i] != 0;
    if (any_ore) { /* sale table: rock, quantity, price each, line total */
        text(l, "Rock", 22, y, 70, 14, INK_DIM, &lv_font_montserrat_10);
        text_align(l, "Qty", 96, y, 26, 14, INK_DIM, &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);
        text_align(l, "Price", 124, y, 40, 14, INK_DIM, &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);
        text_align(l, "Total", 166, y, 52, 14, INK_DIM, &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);
        y += 14;
    }
    for (unsigned i = 0; i < GD_ORE_KINDS; ++i) {
        if (!rep->ore[i]) continue;
        const GdTileInfo *info = gd_tile_info(GD_ORE_FIRST + i);
        const uint64_t each = gd_ore_value(&game.profile, i);
        box(l, 22, y + 3, 8, 8, info->color, shade(info->color, 55), LV_OPA_COVER, 1);
        text(l, info->name, 34, y, 62, 14, INK_BODY, &lv_font_montserrat_12);
        snprintf(s, sizeof(s), "%u", rep->ore[i]);
        text_align(l, s, 96, y, 26, 14, INK_BODY, &lv_font_montserrat_12, LV_TEXT_ALIGN_RIGHT);
        amount(each, v, sizeof(v));
        text_align(l, v, 124, y, 40, 14, INK_BODY, &lv_font_montserrat_12, LV_TEXT_ALIGN_RIGHT);
        amount(rep->ore[i] * each, v, sizeof(v));
        text_align(l, v, 166, y, 52, 14, INK_TITLE, &lv_font_montserrat_12, LV_TEXT_ALIGN_RIGHT);
        y += 18;
    }
    if (rep->chest_gold) {
        coin(l, 21, y + 2, 10);
        text(l, "Treasure", 36, y, 120, 14, INK_BODY, &lv_font_montserrat_12);
        amount(rep->chest_gold, v, sizeof(v));
        text_align(l, v, 166, y, 52, 14, INK_TITLE, &lv_font_montserrat_12, LV_TEXT_ALIGN_RIGHT);
        y += 18;
    }
    if (y == 64) {
        text(l, rep->end == GD_END_ROPE ? "Empty bag this time." : "Nothing made it out.", 22, y, 196, 14, INK_DIM,
             &lv_font_montserrat_12);
        y += 18;
    }
    if (rep->lost) {
        snprintf(s, sizeof(s), tr(rep->lost == 1 ? "Lost %u piece of ore" : "Lost %u pieces of ore"), rep->lost);
        text(l, s, 22, y, 196, 14, INK_BAD, &lv_font_montserrat_12);
        y += 18;
    }
    line(l, 20, y + 4, 220, y + 4, BRASS_DIM, 1, LV_OPA_COVER);
    amount(rep->total, v, sizeof(v));
    snprintf(s, sizeof(s), "+%s", v);
    text(l, "Earned", 22, y + 10, 100, 18, INK_BODY, &lv_font_montserrat_14);
    coin(l, 196, y + 13, 12);
    text_align(l, s, 100, y + 10, 92, 18, INK_TITLE, &lv_font_montserrat_14, LV_TEXT_ALIGN_RIGHT);
    snprintf(s, sizeof(s), tr("Deepest %u m"), rep->deepest);
    text(l, s, 22, y + 36, 120, 16, INK_BODY, &lv_font_montserrat_12);
    if (rep->new_best && (clock_ms / 400) % 2 == 0)
        text_align(l, "NEW BEST!", 120, y + 36, 98, 16, INK_GOOD, &lv_font_montserrat_12, LV_TEXT_ALIGN_RIGHT);
    hit_add(0, 30, 240, 290, HIT_ROW);
    footer(l, pointer_mode ? "Tap to continue" : "OK continue");
}
static void draw_shop(lv_layer_t *l) {
    backdrop(l);
    header(l, "Trading Post", true);
    char labels[SHOP_ROWS][40], values[SHOP_ROWS][24];
    uint32_t inks[SHOP_ROWS];
    for (unsigned u = 0; u < GD_UPGRADE_COUNT; ++u) {
        uint64_t price = gd_upgrade_price(&game.profile, (GdUpgrade)u);
        if (u == GD_DYNAMITE)
            snprintf(labels[u], 40, "%s  x%u", tr(gd_upgrade_name((GdUpgrade)u)), game.profile.level[u]);
        else
            snprintf(labels[u], 40, tr("%s  Lv %u"), tr(gd_upgrade_name((GdUpgrade)u)), game.profile.level[u] + 1);
        if (!price)
            snprintf(values[u], 24, "%s", u == GD_DYNAMITE ? "Full" : "Max");
        else
            amount(price, values[u], 24);
        inks[u] = !price ? INK_GOOD : price <= game.profile.gold ? INK_TITLE : INK_BAD;
    }
    snprintf(labels[GD_UPGRADE_COUNT], 40, "Back");
    values[GD_UPGRADE_COUNT][0] = 0;
    rows(l, 34, SHOP_ROWS, SHOP_ROWS, labels, values, inks);
    panel(l, 8, 218, 224, 72);
    if (selected < GD_UPGRADE_COUNT) {
        const GdProfile *p = &game.profile;
        GdProfile next = *p;
        if (next.level[selected] < gd_upgrade_max((GdUpgrade)selected)) ++next.level[selected];
        char s[64];
        text(l, gd_upgrade_blurb((GdUpgrade)selected), 16, 224, 208, 30, INK_BODY, &lv_font_montserrat_12);
        switch (selected) {
            case GD_PICK:
                snprintf(s, sizeof(s), tr("Stone %u hits  ->  %u"), gd_hits_for(p, gd_tile_info(GD_STONE)->hardness),
                         gd_hits_for(&next, gd_tile_info(GD_STONE)->hardness));
                break;
            case GD_LANTERN:
                snprintf(s, sizeof(s), tr("Light radius %u  ->  %u"), gd_light_radius(p), gd_light_radius(&next));
                break;
            case GD_ENERGY: snprintf(s, sizeof(s), tr("Energy %u  ->  %u"), gd_energy_max(p), gd_energy_max(&next)); break;
            case GD_BAG: snprintf(s, sizeof(s), tr("Holds %u  ->  %u pieces"), gd_bag_capacity(p), gd_bag_capacity(&next)); break;
            case GD_CHARM: snprintf(s, sizeof(s), tr("Ore value +%u%%  ->  +%u%%"), gd_charm_percent(p), gd_charm_percent(&next)); break;
            default: snprintf(s, sizeof(s), tr("Owned %u of %u"), p->level[GD_DYNAMITE], GD_DYNAMITE_MAX); break;
        }
        text(l, s, 16, 266, 208, 16, INK_GOOD, &lv_font_montserrat_10);
    } else
        text(l, "Return to the surface camp.", 16, 224, 208, 30, INK_BODY, &lv_font_montserrat_12);
    footer(l, pointer_mode ? "Tap to buy" : "OK buy   hold OK back");
}
static void draw_elevator(lv_layer_t *l) {
    backdrop(l);
    header(l, "Elevator", true);
    char labels[GD_STOP_COUNT + 1][40], values[GD_STOP_COUNT + 1][24];
    uint32_t inks[GD_STOP_COUNT + 1];
    for (unsigned s = 0; s < GD_STOP_COUNT; ++s) {
        if (s)
            snprintf(labels[s], 40, tr("Stop  %u m"), s * GD_STOP_SPACING);
        else
            snprintf(labels[s], 40, "Surface  0 m");
        if (gd_stop_owned(&game.profile, s)) {
            snprintf(values[s], 24, "Ride");
            inks[s] = INK_GOOD;
        } else if (gd_stop_reached(&game.profile, s)) {
            amount(gd_stop_price(s), values[s], 24);
            inks[s] = gd_stop_price(s) <= game.profile.gold ? INK_TITLE : INK_BAD;
        } else {
            snprintf(values[s], 24, "Locked");
            inks[s] = INK_DIM;
        }
    }
    snprintf(labels[GD_STOP_COUNT], 40, "Back");
    values[GD_STOP_COUNT][0] = 0;
    rows(l, 36, GD_STOP_COUNT + 1, 6, labels, values, inks);
    panel(l, 8, 222, 224, 68);
    char s[80];
    if (selected >= GD_STOP_COUNT)
        snprintf(s, sizeof(s), "Return to the surface camp.");
    else if (gd_stop_owned(&game.profile, selected))
        snprintf(s, sizeof(s), tr("Ride down and start digging at %u m."), selected * GD_STOP_SPACING);
    else if (gd_stop_reached(&game.profile, selected))
        snprintf(s, sizeof(s), "Open this stop once; ride it on every later dive.");
    else
        snprintf(s, sizeof(s), tr("Reach %u m in a dive to open this stop."), selected * GD_STOP_SPACING);
    text(l, s, 16, 230, 208, 40, INK_BODY, &lv_font_montserrat_12);
    snprintf(s, sizeof(s), tr("Best depth %u m"), game.profile.best_depth);
    text(l, s, 16, 270, 208, 14, INK_DIM, &lv_font_montserrat_10);
    footer(l, pointer_mode ? "Tap a stop" : "OK ride / open   hold OK back");
}
static void draw_settings(lv_layer_t *l) {
    backdrop(l);
    header(l, "Settings", false);
    static const char *const sleeps[5] = {"30 s", "1 min", "2 min", "5 min", "Never"};
    char labels[SETTINGS_ROWS][40], values[SETTINGS_ROWS][24] = {{0}};
    snprintf(labels[0], 40, "Music");
    snprintf(values[0], 24, "%u%%", preferences[PREF_MUSIC]);
    snprintf(labels[1], 40, "Sound effects");
    snprintf(values[1], 24, "%u%%", preferences[PREF_EFFECTS]);
    snprintf(labels[2], 40, "Brightness");
    snprintf(values[2], 24, "%u%%", preferences[PREF_BRIGHTNESS]);
    snprintf(labels[3], 40, "Sleep when idle");
    snprintf(values[3], 24, "%s", tr(pointer_mode ? "Passport only" : sleeps[preferences[PREF_SLEEP]]));
    snprintf(labels[4], 40, "Language");
    snprintf(values[4], 24, "%s", language_names[language]);
    snprintf(labels[5], 40, "Erase progress");
    snprintf(labels[6], 40, "Back");
    rows(l, 34, SETTINGS_ROWS, 6, labels, values, NULL);
    panel(l, 8, 222, 224, 68);
    char s[64];
    snprintf(s, sizeof(s), tr("Gold Digger %s"), firmware_version());
    text(l, s, 16, 228, 208, 16, INK_TITLE, &lv_font_montserrat_12);
    text(l, save_enabled ? "Progress saves automatically." : "Saving is off for this session.", 16, 248, 208, 14,
         save_enabled ? INK_BODY : INK_BAD, &lv_font_montserrat_10);
    text(l, "Original game for FoloToy AI Passport.", 16, 266, 208, 14, INK_DIM, &lv_font_montserrat_10);
    footer(l, pointer_mode ? "Tap to change" : "OK change   hold OK back");
}
static void draw_reset(lv_layer_t *l) {
    backdrop(l);
    header(l, "Erase progress", false);
    panel(l, 10, 40, 220, 100);
    text(l, "Erase all gold, upgrades, elevator stops and records?", 20, 50, 200, 40, INK_BODY,
         &lv_font_montserrat_12);
    text(l, "This cannot be undone.", 20, 100, 200, 16, INK_BAD, &lv_font_montserrat_12);
    char labels[2][40] = {"Keep my progress", "Erase everything"};
    rows(l, 160, 2, 2, labels, NULL, NULL);
    footer(l, pointer_mode ? "Tap an option" : "UP/DOWN choose   OK confirm");
}

/* First start: each language is named in itself; the highlight previews it. */
static void draw_language(lv_layer_t *l) {
    backdrop(l);
    header(l, "Gold Digger", false);
    panel(l, 10, 40, 220, 70);
    text_align(l, "Choose a language", 20, 52, 200, 18, INK_TITLE, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
    text_align(l, "请选择语言", 20, 78, 200, 18, INK_TITLE, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);
    char labels[LANGUAGE_COUNT][40], values[LANGUAGE_COUNT][24] = {{0}};
    for (unsigned i = 0; i < LANGUAGE_COUNT; ++i) snprintf(labels[i], 40, "%s", language_names[i]);
    rows(l, 130, LANGUAGE_COUNT, LANGUAGE_COUNT, labels, values, NULL);
    footer(l, pointer_mode ? "Tap a language" : "UP/DOWN choose   OK confirm");
}

static void draw_event(lv_event_t *event) {
    lv_layer_t *l = lv_event_get_layer(event);
    hit_clear();
    switch (page) {
        case HOME: draw_home(l); break;
        case MINE: draw_mine(l); break;
        case PAUSE: draw_pause(l); break;
        case REPORT: draw_report(l); break;
        case SHOP: draw_shop(l); break;
        case ELEVATOR: draw_elevator(l); break;
        case SETTINGS: draw_settings(l); break;
        case LANGUAGE_PICK: draw_language(l); break;
        default: draw_reset(l); break;
    }
}
void build_ui(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0b0d0e), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    screen_obj = lv_obj_create(scr);
    lv_obj_remove_style_all(screen_obj);
    lv_obj_set_size(screen_obj, 240, 320);
    lv_obj_set_pos(screen_obj, 0, 0);
    lv_obj_remove_flag(screen_obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen_obj, draw_event, LV_EVENT_DRAW_MAIN, NULL);
}
void render(void) {
    if (screen_obj) lv_obj_invalidate(screen_obj);
}
