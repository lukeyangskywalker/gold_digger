/* Every Gold Digger screen, drawn with LVGL primitives on one full-screen
   object. Palette and chrome follow Spartan: dark glass panels (0x101519)
   with brass borders, warm parchment text and a gold coin for balances. */
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
static void circle(lv_layer_t *l, int cx, int cy, int r, uint32_t color, lv_opa_t opa) {
    box(l, cx - r, cy - r, 2 * r + 1, 2 * r + 1, color, color, opa, LV_RADIUS_CIRCLE);
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
static void triangle(lv_layer_t *l, int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color, lv_opa_t opa) {
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.color = lv_color_hex(color);
    d.opa = opa;
    d.p[0] = (lv_point_precise_t){x1, y1};
    d.p[1] = (lv_point_precise_t){x2, y2};
    d.p[2] = (lv_point_precise_t){x3, y3};
    lv_draw_triangle(l, &d);
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
static void coin(lv_layer_t *l, int x, int y, int size) {
    box(l, x, y, size, size, 0xf2c037, 0x8a5a12, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    int inset = size >= 12 ? 3 : 2;
    box(l, x + inset, y + inset, size - 2 * inset, size - 2 * inset, 0xffde6e, 0xffde6e, LV_OPA_COVER,
        LV_RADIUS_CIRCLE);
    fill(l, x + size / 4, y + size / 4, size >= 12 ? 2 : 1, size >= 12 ? 2 : 1, 0xfff6cf, LV_OPA_COVER);
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
static void header(lv_layer_t *l, const char *title, bool gold) {
    box(l, 3, 3, 234, 22, PANEL, BRASS, 245, 0);
    int x = 9;
    if (pointer_mode && page != HOME && page != MINE && page != LANGUAGE_PICK) {
        box(l, 5, 5, 22, 18, ROW, ROW_EDGE, LV_OPA_COVER, 3);
        text_align(l, LV_SYMBOL_LEFT, 5, 7, 22, 16, FOCUS_INK, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
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
        box(l, 212, 9, 17, 10, 0x18201c, 0xb8be9e, LV_OPA_COVER, 0);
        fill(l, 229, 12, 2, 4, 0xb8be9e, LV_OPA_COVER);
        fill(l, 214, 11, 13 * p / 100 + 1, 6, battery_charging ? 0xffe16b : p < 20 ? 0xcc7959 : 0x9cbb78,
             LV_OPA_COVER);
    }
}
static void footer(lv_layer_t *l, const char *hint) {
    box(l, 3, 296, 234, 21, PANEL, BRASS_DIM, 245, 0);
    bool fresh = notice[0] && clock_ms < notice_until;
    text_align(l, fresh ? notice : hint, 8, 300, 224, 14, fresh ? INK_TITLE : INK_HINT, &lv_font_montserrat_10,
               LV_TEXT_ALIGN_CENTER);
}
/* Underground backdrop for menus: dim strata with scattered ore glints. */
static void backdrop(lv_layer_t *l) {
    for (int y = 0; y < 320; y += 16) {
        GdTile t = gd_stratum((unsigned)(y * GD_ROWS / 320));
        fill(l, 0, y, 240, 16, shade(gd_tile_info(t)->color, 30 + (y / 16 % 2) * 4), LV_OPA_COVER);
    }
    for (unsigned i = 0; i < 26; ++i) {
        uint32_t h = hash(i, 77);
        int x = (int)(h % 236), y = (int)((h >> 8) % 316);
        GdTile ore = (GdTile)(GD_COPPER + (h >> 20) % 5);
        fill(l, x, y, 2 + (h >> 28) % 2, 2, shade(gd_tile_info(ore)->color, 55), LV_OPA_COVER);
    }
}
/* A list of rows in Spartan's style; returns the bottom edge. */
static int rows(lv_layer_t *l, int y0, unsigned count, unsigned visible, char labels[][40], char values[][24],
                const uint32_t *value_ink) {
    unsigned first = 0;
    if (count > visible && selected >= visible / 2) first = selected - visible / 2;
    if (first + visible > count) first = count > visible ? count - visible : 0;
    int y = y0;
    for (unsigned i = first; i < count && i < first + visible; ++i, y += 30) {
        bool focus = i == selected;
        box(l, 10, y, 220, 26, focus ? FOCUS : ROW, focus ? FOCUS_EDGE : ROW_EDGE, 245, 3);
        text(l, labels[i], 18, y + 6, 140, 16, focus ? FOCUS_INK : ROW_INK, &lv_font_montserrat_12);
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
static void panel(lv_layer_t *l, int x, int y, int w, int h) { box(l, x, y, w, h, PANEL, BRASS_DIM, 240, 4); }

/* ---- the miner -------------------------------------------------------------- */
static void miner(lv_layer_t *l, int x, int y, bool right, bool swing) {
    /* (x, y) = top-left of a 30x30 cell; the miner is about 18 px wide. */
    int cx = x + 15, dir = right ? 1 : -1;
    /* lamp beam */
    triangle(l, cx + dir * 6, y + 7, cx + dir * 28, y + 1, cx + dir * 28, y + 15, 0xfff2b0, 60);
    fill(l, cx - 5, y + 25, 4, 4, 0x3a2a1c, LV_OPA_COVER); /* boots */
    fill(l, cx + 1, y + 25, 4, 4, 0x3a2a1c, LV_OPA_COVER);
    box(l, cx - 6, y + 14, 12, 12, 0x3a5a8a, 0x23395a, LV_OPA_COVER, 2); /* overalls */
    fill(l, cx - 4, y + 15, 8, 3, 0xc94b3c, LV_OPA_COVER);                  /* shirt */
    box(l, cx - 5, y + 6, 10, 9, 0xe5b48a, 0xb88560, LV_OPA_COVER, 3);      /* face */
    fill(l, cx + dir * 2, y + 9, 2, 2, 0x221a14, LV_OPA_COVER);             /* eye */
    box(l, cx - 7, y + 2, 14, 6, 0xf2c037, 0x8a5a12, LV_OPA_COVER, 3);     /* helmet */
    fill(l, cx - 8, y + 7, 16, 2, 0xc9971f, LV_OPA_COVER);                  /* brim */
    circle(l, cx + dir * 5, y + 4, 2, 0xfffbe0, LV_OPA_COVER);              /* lamp */
    /* pickaxe */
    int hx = cx + dir * 7, hy = y + 20;
    int tx = swing ? cx + dir * 12 : cx + dir * 13, ty = swing ? y + 6 : y + 26;
    line(l, hx, hy, tx, ty, 0x8a5a32, 2, LV_OPA_COVER);
    if (swing)
        line(l, tx - dir * 6, ty - 2, tx + dir * 5, ty + 3, 0xb9c2c8, 2, LV_OPA_COVER);
    else
        line(l, tx - 4, ty - dir * 0 + 1, tx + 4, ty - 3, 0xb9c2c8, 2, LV_OPA_COVER);
}

/* ---- surface ---------------------------------------------------------------- */
static const int station_x[STATION_COUNT] = {38, 104, 164, 214};
static const char *const station_name[STATION_COUNT] = {"Trading Post", "Mine Shaft", "Elevator", "Settings"};
static void draw_home(lv_layer_t *l) {
    static const uint32_t sky[6] = {0x141a2c, 0x1a2136, 0x222840, 0x2c2e48, 0x3a344e, 0x4a3c50};
    for (int i = 0; i < 6; ++i) fill(l, 0, i * 28, 240, 28, sky[i], LV_OPA_COVER);
    for (unsigned i = 0; i < 22; ++i) {
        uint32_t h = hash(i, 5);
        int x = (int)(h % 240), y = 28 + (int)((h >> 9) % 110);
        bool on = ((clock_ms / 400 + i) % 7) != 0;
        fill(l, x, y, 1 + (h >> 30 == 0), 1, 0xfff6cf, on ? 220 : 90);
    }
    circle(l, 200, 52, 11, 0xf4e6c0, LV_OPA_COVER); /* moon */
    circle(l, 205, 48, 10, sky[1], LV_OPA_COVER);
    circle(l, 40, 196, 70, 0x232a30, LV_OPA_COVER); /* hills */
    circle(l, 170, 210, 90, 0x1d2328, LV_OPA_COVER);
    /* ground cross-section */
    fill(l, 0, 166, 240, 6, 0x4f6b3a, LV_OPA_COVER);
    for (int y = 172, band = 0; y < 296; y += 14, ++band) {
        GdTile t = gd_stratum((unsigned)band * 40);
        fill(l, 0, y, 240, 14, shade(gd_tile_info(t)->color, 70), LV_OPA_COVER);
    }
    for (unsigned i = 0; i < 18; ++i) {
        uint32_t h = hash(i, 9);
        GdTile ore = (GdTile)(GD_COAL + (h >> 24) % 6);
        fill(l, (int)(h % 232), 176 + (int)((h >> 8) % 112), 4, 3, gd_tile_info(ore)->color, LV_OPA_COVER);
    }
    fill(l, 96, 166, 16, 130, 0x0b0d0e, LV_OPA_COVER); /* shaft */
    /* trading post */
    fill(l, 14, 132, 50, 36, 0x7a5232, LV_OPA_COVER);
    triangle(l, 8, 134, 39, 110, 70, 134, 0x5a3424, LV_OPA_COVER);
    fill(l, 32, 146, 13, 22, 0x3a2418, LV_OPA_COVER);
    coin(l, 45, 120, 12);
    /* headframe */
    line(l, 88, 168, 104, 112, 0x8a6a42, 3, LV_OPA_COVER);
    line(l, 120, 168, 104, 112, 0x8a6a42, 3, LV_OPA_COVER);
    line(l, 93, 150, 115, 150, 0x8a6a42, 2, LV_OPA_COVER);
    line(l, 98, 132, 110, 132, 0x8a6a42, 2, LV_OPA_COVER);
    box(l, 95, 103, 19, 19, 0x2b2b2b, 0xb9a27a, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    {
        unsigned a = (clock_ms / 120) % 4;
        line(l, 104 - (int)(a == 0) * 8 + (int)(a == 2) * 8, 112 - (int)(a == 1) * 8 + (int)(a == 3) * 8,
             104 + (int)(a == 0) * 8 - (int)(a == 2) * 8, 112 + (int)(a == 1) * 8 - (int)(a == 3) * 8, 0xb9a27a,
             1, LV_OPA_COVER);
    }
    line(l, 104, 121, 104, 290, 0xb9a27a, 1, LV_OPA_COVER);
    /* elevator tower with a bobbing cage */
    line(l, 152, 168, 152, 100, 0x6d7a80, 2, LV_OPA_COVER);
    line(l, 176, 168, 176, 100, 0x6d7a80, 2, LV_OPA_COVER);
    line(l, 150, 100, 178, 100, 0x6d7a80, 3, LV_OPA_COVER);
    {
        int bob = (int)((clock_ms / 90) % 40);
        int cy = 112 + (bob < 20 ? bob : 40 - bob);
        line(l, 164, 100, 164, cy, 0xb9c2c8, 1, LV_OPA_COVER);
        box(l, 156, cy, 16, 18, 0x2e3a3f, 0x9aa8ae, LV_OPA_COVER, 1);
    }
    /* settings signpost */
    fill(l, 212, 132, 4, 36, 0x6a4a2a, LV_OPA_COVER);
    box(l, 196, 124, 36, 16, 0x8a6a42, 0x5a3a1a, LV_OPA_COVER, 2);
    text_align(l, LV_SYMBOL_SETTINGS, 196, 125, 36, 14, 0x2a1a0a, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
    /* stations, selection and the miner */
    for (unsigned s = 0; s < STATION_COUNT; ++s) hit_add(station_x[s] - 26, 90, 52, 82, HIT_ROW + (int)s);
    int sx = station_x[selected];
    int bounce = (int)((clock_ms / 150) % 4);
    text_align(l, LV_SYMBOL_DOWN, sx - 10, 84 + (bounce < 2 ? bounce : 4 - bounce), 20, 14, INK_TITLE,
               &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
    miner(l, sx - 15 + (selected == STATION_SETTINGS ? -14 : 14), 139, selected == STATION_SETTINGS, false);
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
static void nuggets(lv_layer_t *l, int x, int y, uint32_t seed, uint32_t color, unsigned count, int size) {
    for (unsigned i = 0; i < count; ++i) {
        uint32_t h = hash(seed, i + 11);
        int nx = x + 4 + (int)(h % (unsigned)(TILE - 8 - size)), ny = y + 4 + (int)((h >> 10) % (unsigned)(TILE - 8 - size));
        box(l, nx, ny, size, size, color, shade(color, 55), LV_OPA_COVER, 1);
        fill(l, nx + 1, ny + 1, 1, 1, shade(color, 160), LV_OPA_COVER);
    }
}
static void draw_tile(lv_layer_t *l, GdTile t, int px, int py, int x, int y) {
    uint32_t seed = hash((uint32_t)x, (uint32_t)y);
    uint32_t rock = gd_tile_info(gd_stratum((unsigned)y))->color;
    if (t == GD_AIR) {
        fill(l, px, py, TILE, TILE, y == 0 ? 0x2c2e48 : shade(rock, 22), LV_OPA_COVER);
        if (y) fill(l, px + (int)(seed % 20), py + (int)((seed >> 8) % 24), 3, 2, shade(rock, 34), LV_OPA_COVER);
        return;
    }
    if (t == GD_LAVA) {
        fill(l, px, py, TILE, TILE, 0xd8401a, LV_OPA_COVER);
        for (unsigned i = 0; i < 3; ++i) {
            uint32_t h = hash(seed, i);
            int bx = px + 3 + (int)(h % 22), by = py + 3 + (int)((h >> 8) + clock_ms / 60) % 22;
            circle(l, bx, by, 2 + (int)(h >> 30), 0xffb02e, LV_OPA_COVER);
        }
        fill(l, px, py, TILE, 3, 0xffd34a, LV_OPA_COVER);
        return;
    }
    if (t == GD_BEDROCK) {
        fill(l, px, py, TILE, TILE, 0x16161a, LV_OPA_COVER);
        for (int k = 0; k < TILE; k += 8) line(l, px + k, py + TILE - 1, px + k + 8, py + 1, 0x2a2a33, 1, LV_OPA_COVER);
        return;
    }
    GdTile base = t <= GD_OBSIDIAN ? t : gd_stratum((unsigned)y);
    rock = gd_tile_info(base)->color;
    box(l, px, py, TILE, TILE, rock, shade(rock, 75), LV_OPA_COVER, 0);
    for (unsigned i = 0; i < 4; ++i) {
        uint32_t h = hash(seed, i + 3);
        fill(l, px + 2 + (int)(h % 25), py + 2 + (int)((h >> 8) % 25), 2 + (int)(h >> 31), 2,
             shade(rock, i & 1 ? 125 : 70), LV_OPA_COVER);
    }
    switch (t) {
        case GD_COAL: nuggets(l, px, py, seed, 0x1a1a1a, 4, 5); break;
        case GD_COPPER: nuggets(l, px, py, seed, 0xd27a3c, 4, 4); break;
        case GD_SILVER: nuggets(l, px, py, seed, 0xd8dde2, 4, 4); break;
        case GD_GOLD: nuggets(l, px, py, seed, 0xf2c037, 3, 6); break;
        case GD_RUBY: nuggets(l, px, py, seed, 0xe0344e, 3, 5); break;
        case GD_DIAMOND: {
            nuggets(l, px, py, seed, 0x7fe8ff, 3, 5);
            if ((clock_ms / 200 + seed) % 6 == 0) fill(l, px + 14, py + 8, 2, 2, 0xffffff, LV_OPA_COVER);
            break;
        }
        case GD_CHEST:
            box(l, px + 6, py + 10, 18, 13, 0x8a5a22, 0x4a2a10, LV_OPA_COVER, 1);
            fill(l, px + 6, py + 14, 18, 2, 0xf2c037, LV_OPA_COVER);
            fill(l, px + 14, py + 15, 3, 4, 0xffde6e, LV_OPA_COVER);
            break;
        case GD_GAS:
            if (gd_sees_gas(&game.profile))
                for (unsigned i = 0; i < 3; ++i) {
                    uint32_t h = hash(seed, i + 40);
                    circle(l, px + 6 + (int)(h % 18), py + 6 + (int)((h >> 8) % 18), 3, 0x9fd36a, 170);
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
    int jx = clock_ms < shake_until ? (int)(hash(clock_ms, 1) % 5) - 2 : 0;
    unsigned radius = gd_light_radius(&game.profile);
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
            if (y > 0 && d > (int)radius) fill(l, px, py, TILE, TILE, 0x000000, d > (int)radius + 1 ? 215 : 150);
            /* The tile being picked at shows cracks, more with each hit. */
            if (r->crack_hits && r->crack_x == x && r->crack_y == y) {
                unsigned n = r->crack_hits;
                line(l, px + 6, py + 1, px + 4, py + 6, 0x111111, 1, LV_OPA_COVER);
                if (n > 1) line(l, px + 4, py + 6, px + 8, py + 10, 0x111111, 1, LV_OPA_COVER);
                if (n > 2) line(l, px + 4, py + 6, px + 1, py + 9, 0x111111, 1, LV_OPA_COVER);
                if (n > 3) line(l, px + 8, py + 3, px + 11, py + 7, 0x111111, 1, LV_OPA_COVER);
            }
            /* Neighbours that can never be dug get a red cross. */
            if (d == 1 && (dx == 0 || dy == 0) && y >= r->y) {
                GdTile t = gd_tile(r, x, y);
                if (t == GD_LAVA || t == GD_BEDROCK)
                {
                    line(l, px + 3, py + 3, px + 9, py + 9, 0xe07a5f, 2, LV_OPA_COVER);
                    line(l, px + 9, py + 3, px + 3, py + 9, 0xe07a5f, 2, LV_OPA_COVER);
                }
            }
        }
        if (y > 0 && y % 10 == 0) {
            char m[8];
            snprintf(m, sizeof(m), "%dm", y);
            text(l, m, 2, py + 1, 30, 10, INK_HINT, &lv_font_montserrat_8);
        }
        if (y == 0) /* grass lip and the rope back up */
            fill(l, 0, py + TILE - 3, 240, 3, 0x4f6b3a, LV_OPA_COVER);
    }
    int mx = r->x * TILE + jx, my = MINE_TOP + ((int)r->y - top) * TILE;
    line(l, mx + 15, MINE_TOP, mx + 15, my + 2, 0xb9a27a, 1, 120);
    miner(l, mx, my, r->facing, clock_ms < shake_until + 60);
    for (unsigned i = 0; i < FLOATERS; ++i) {
        Floater *f = &floaters[i];
        if (!f->start || clock_ms - f->start > 900) continue;
        int rise = (int)(clock_ms - f->start) / 30;
        int fx = f->x * TILE + 15 - 40, fy = MINE_TOP + ((int)f->row - top) * TILE - 6 - rise;
        text_align(l, f->text, fx + 1, fy + 1, 80, 14, 0x000000, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
        text_align(l, f->text, fx, fy, 80, 14, f->color, &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
    }
    /* HUD */
    box(l, 3, 3, 234, 21, PANEL, BRASS, 245, 0);
    char s[32];
    snprintf(s, sizeof(s), tr("%u m"), r->y);
    text(l, s, 9, 7, 50, 14, INK_TITLE, &lv_font_montserrat_12);
    int bar = r->energy_max ? 74 * r->energy / r->energy_max : 0;
    box(l, 62, 9, 78, 10, 0x18201c, 0xb8be9e, LV_OPA_COVER, 2);
    fill(l, 64, 11, bar, 6,
         r->energy * 5 <= r->energy_max ? 0xcc5a4a : r->energy * 2 <= r->energy_max ? 0xe0b04a : 0x9cbb78,
         LV_OPA_COVER);
    snprintf(s, sizeof(s), "%u/%u", r->energy, r->energy_max); /* one swing = one energy */
    text_align(l, s, 63, 8, 78, 12, 0x000000, &lv_font_montserrat_10, LV_TEXT_ALIGN_CENTER);
    text_align(l, s, 62, 7, 78, 12, 0xffffff, &lv_font_montserrat_10, LV_TEXT_ALIGN_CENTER);
    snprintf(s, sizeof(s), "%u/%u", gd_bag_used(r), r->bag_cap);
    box(l, 150, 8, 11, 12, 0x8a6a42, 0x5a3a1a, LV_OPA_COVER, 3); /* bag */
    text(l, s, 165, 7, 44, 14, gd_bag_used(r) >= r->bag_cap ? INK_BAD : INK_BODY, &lv_font_montserrat_12);
    snprintf(s, sizeof(s), "x%u", game.profile.level[GD_DYNAMITE]);
    fill(l, 208, 9, 4, 11, 0xc94b3c, LV_OPA_COVER);
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
        box(l, 186, 298, 48, 17, FOCUS, FOCUS_EDGE, LV_OPA_COVER, 3);
        text_align(l, "Menu", 186, 300, 48, 14, FOCUS_INK, &lv_font_montserrat_10, LV_TEXT_ALIGN_CENTER);
        hit_add(180, 292, 60, 28, HIT_MENU);
    } else
        footer(l, "UP left  DOWN right  OK dig  hold OK menu");
}
static void draw_pause(lv_layer_t *l) {
    draw_mine(l);
    hit_clear();
    fill(l, 0, 0, 240, 320, 0x000000, 150);
    box(l, 4, 70, 232, 176, PANEL, FOCUS_EDGE, 250, 6);
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
