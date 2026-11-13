/* Gold Digger rules: mine generation, digging, hazards, upgrades, elevator
   stops and the versioned save format. Pure C11, deterministic per seed. */
#include "digger.h"
#include <string.h>

static const GdTileInfo tiles[GD_TILE_COUNT] = {
        [GD_AIR] = {"Air", 0, 0, 0x0b0d0e},
        [GD_DIRT] = {"Dirt", 1, 0, 0x6b4a2e},
        [GD_CLAY] = {"Clay", 2, 0, 0x8a5a3a},
        [GD_STONE] = {"Stone", 3, 0, 0x6d6a64},
        [GD_GRANITE] = {"Granite", 4, 0, 0x7c6a6a},
        [GD_BASALT] = {"Basalt", 5, 0, 0x3e4148},
        [GD_OBSIDIAN] = {"Obsidian", 6, 0, 0x2a2238},
        [GD_BEDROCK] = {"Bedrock", 255, 0, 0x16161a},
        [GD_COAL] = {"Coal", 0, 2, 0x202020},
        [GD_COPPER] = {"Copper", 0, 5, 0xd27a3c},
        [GD_SILVER] = {"Silver", 0, 12, 0xd8dde2},
        [GD_GOLD] = {"Gold", 0, 30, 0xf2c037},
        [GD_RUBY] = {"Ruby", 0, 70, 0xe0344e},
        [GD_DIAMOND] = {"Diamond", 0, 160, 0x7fe8ff},
        [GD_CHEST] = {"Chest", 0, 0, 0x9a6a2a},
        [GD_GAS] = {"Gas pocket", 0, 0, 0x7fa25a},
        [GD_LAVA] = {"Lava", 255, 0, 0xff5a1f},
};
const GdTileInfo *gd_tile_info(GdTile tile) { return &tiles[(unsigned)tile < GD_TILE_COUNT ? tile : GD_BEDROCK]; }

static const char *const upgrade_names[GD_UPGRADE_COUNT] = {"Pickaxe", "Lantern", "Stamina", "Ore bag", "Lucky charm",
                                                            "Dynamite"};
static const char *const upgrade_blurbs[GD_UPGRADE_COUNT] = {
        "Fewer hits per tile",
        "Wider light; reveals gas at level 3",
        "More energy per dive",
        "Carries more ore",
        "+5% ore value per level",
        "Blasts the 3x3 around you (pause menu)",
};
static const uint8_t upgrade_max[GD_UPGRADE_COUNT] = {4, 8, 8, 8, 10, GD_DYNAMITE_MAX};
const char *gd_upgrade_name(GdUpgrade u) { return (unsigned)u < GD_UPGRADE_COUNT ? upgrade_names[u] : ""; }
const char *gd_upgrade_blurb(GdUpgrade u) { return (unsigned)u < GD_UPGRADE_COUNT ? upgrade_blurbs[u] : ""; }
unsigned gd_upgrade_max(GdUpgrade u) { return (unsigned)u < GD_UPGRADE_COUNT ? upgrade_max[u] : 0; }

static uint64_t grow(uint64_t base, unsigned num, unsigned den, unsigned times) {
    uint64_t v = base;
    while (times--) v = (v * num + den - 1) / den;
    return v;
}
uint64_t gd_upgrade_price(const GdProfile *p, GdUpgrade u) {
    static const uint64_t pick[4] = {120, 600, 2500, 9000};
    unsigned level = p->level[u];
    if ((unsigned)u >= GD_UPGRADE_COUNT || level >= upgrade_max[u]) return 0;
    switch (u) {
        case GD_PICK: return pick[level];
        case GD_LANTERN: return grow(60, 17, 10, level);
        case GD_ENERGY: return grow(45, 17, 10, level);
        case GD_BAG: return grow(40, 17, 10, level);
        case GD_CHARM: return grow(200, 19, 10, level);
        default: return 35;
    }
}
bool gd_buy_upgrade(GdGame *g, GdUpgrade u) {
    uint64_t price = gd_upgrade_price(&g->profile, u);
    if (!price || g->profile.gold < price) return false;
    if (g->run.active && u != GD_DYNAMITE) return false; /* gear changes only on the surface */
    g->profile.gold -= price;
    ++g->profile.level[u];
    return true;
}
unsigned gd_pick_power(const GdProfile *p) { return p->level[GD_PICK]; }
unsigned gd_hits_for(const GdProfile *p, unsigned hardness) {
    if (!hardness || hardness == 255) return 0;
    unsigned power = gd_pick_power(p);
    return hardness > power + 1 ? hardness - power : 1;
}
unsigned gd_energy_max(const GdProfile *p) { return 30u + 15u * p->level[GD_ENERGY]; }
unsigned gd_bag_capacity(const GdProfile *p) { return 10u + 6u * p->level[GD_BAG]; }
unsigned gd_charm_percent(const GdProfile *p) { return 5u * p->level[GD_CHARM]; }
unsigned gd_light_radius(const GdProfile *p) { return 1u + (p->level[GD_LANTERN] + 1u) / 2u; }
bool gd_sees_gas(const GdProfile *p) { return p->level[GD_LANTERN] >= 3; }

bool gd_stop_reached(const GdProfile *p, unsigned stop) {
    return stop < GD_STOP_COUNT && p->best_depth >= stop * GD_STOP_SPACING;
}
bool gd_stop_owned(const GdProfile *p, unsigned stop) { return stop < GD_STOP_COUNT && (p->stops >> stop) & 1u; }
uint64_t gd_stop_price(unsigned stop) { return 150ull * stop * stop; }
bool gd_buy_stop(GdGame *g, unsigned stop) {
    if (!gd_stop_reached(&g->profile, stop) || gd_stop_owned(&g->profile, stop)) return false;
    uint64_t price = gd_stop_price(stop);
    if (g->profile.gold < price) return false;
    g->profile.gold -= price;
    g->profile.stops |= (uint8_t)(1u << stop);
    return true;
}

/* ---- generation ----------------------------------------------------------- */
static uint32_t rng_next(uint32_t *s) {
    uint32_t x = *s ? *s : 0x6d2b79f5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}
static unsigned roll(uint32_t *s, unsigned n) { return rng_next(s) % n; }
GdTile gd_stratum(unsigned y) {
    return y < 25    ? GD_DIRT
           : y < 50  ? GD_CLAY
           : y < 100 ? GD_STONE
           : y < 160 ? GD_GRANITE
           : y < 250 ? GD_BASALT
                     : GD_OBSIDIAN;
}
static GdTile stratum(unsigned y) { return gd_stratum(y); }
static unsigned stratum_hardness(unsigned y) { return tiles[stratum(y)].hardness; }
static GdTile rock_at(uint32_t *s, unsigned y) {
    GdTile base = stratum(y);
    /* Soft seams of the layer above reach a little way into each new layer. */
    if (base > GD_DIRT && y >= 8 && stratum(y - 8) != base && roll(s, 100) < 35) return (GdTile)(base - 1);
    return base;
}
static GdTile mineral_at(uint32_t *s, unsigned y) {
    unsigned r = roll(s, 1000), at = 0;
    struct {
        GdTile tile;
        unsigned from, to, per_mille;
    } const table[] = {
            {GD_COAL, 2, 150, 45},       {GD_COPPER, 5, 230, 38},           {GD_SILVER, 40, 330, 26},
            {GD_GOLD, 90, GD_ROWS, 16 + y / 40}, {GD_RUBY, 170, GD_ROWS, 11}, {GD_DIAMOND, 260, GD_ROWS, 8},
            {GD_GAS, 30, GD_ROWS, 14},  {GD_CHEST, 8, GD_ROWS, 3},
    };
    for (unsigned i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
        if (y >= table[i].from && y < table[i].to) {
            at += table[i].per_mille;
            if (r < at) return table[i].tile;
        }
    return GD_AIR; /* none */
}
static uint8_t *cell(GdRun *r, int x, int y) { return &r->tiles[y * GD_COLS + x]; }
static void generate(GdRun *r, uint32_t seed) {
    uint32_t s = seed * 2654435761u + 0x9e3779b9u;
    for (unsigned y = 0; y < GD_ROWS; ++y)
        for (unsigned x = 0; x < GD_COLS; ++x) {
            GdTile t = y == 0 ? GD_AIR : y == GD_ROWS - 1 ? GD_BEDROCK : rock_at(&s, y);
            if (y > 0 && y < GD_ROWS - 1) {
                GdTile m = mineral_at(&s, y);
                if (m != GD_AIR) t = m;
            }
            *cell(r, x, y) = (uint8_t)t;
        }
    /* Caves: short random walks. Below 110 m their floors may hold lava. */
    for (unsigned c = 0; c < GD_ROWS / 9; ++c) {
        int x = (int)roll(&s, GD_COLS), y = 20 + (int)roll(&s, GD_ROWS - 30);
        unsigned steps = 5 + roll(&s, 10);
        for (unsigned i = 0; i < steps; ++i) {
            if (x >= 0 && x < GD_COLS && y > 1 && y < GD_ROWS - 2) *cell(r, x, y) = GD_AIR;
            switch (roll(&s, 4)) {
                case 0: --x; break;
                case 1: ++x; break;
                case 2: ++y; break;
                default: --y; break;
            }
            if (x < 0) x = 0;
            if (x >= GD_COLS) x = GD_COLS - 1;
        }
    }
    for (unsigned y = 110; y < GD_ROWS - 2; ++y)
        for (unsigned x = 0; x < GD_COLS; ++x)
            if (*cell(r, x, y) == GD_AIR && *cell(r, x, y + 1) != GD_AIR && roll(&s, 100) < 45)
                *cell(r, x, y) = GD_LAVA;
    /* Relics rest on the bedrock. */
    *cell(r, roll(&s, GD_COLS), GD_ROWS - 2) = GD_CHEST;
    *cell(r, roll(&s, GD_COLS), GD_ROWS - 2) = GD_CHEST;
}

void gd_new_profile(GdGame *g) {
    memset(&g->profile, 0, sizeof(g->profile));
    g->profile.stops = 1;
    g->profile.next_seed = 1;
    memset(&g->run, 0, sizeof(g->run));
    memset(&g->report, 0, sizeof(g->report));
}
void gd_init(GdGame *g) {
    memset(g, 0, sizeof(*g));
    gd_new_profile(g);
}
GdTile gd_tile(const GdRun *r, int x, int y) {
    if (x < 0 || x >= GD_COLS || y < 0 || y >= GD_ROWS) return GD_BEDROCK;
    return (GdTile)r->tiles[y * GD_COLS + x];
}
unsigned gd_bag_used(const GdRun *r) {
    unsigned n = 0;
    for (unsigned i = 0; i < GD_ORE_KINDS; ++i) n += r->bag[i];
    return n;
}
static unsigned hardness_at(const GdRun *r, int x, int y) {
    GdTile t = gd_tile(r, x, y);
    unsigned h = tiles[t].hardness;
    return h ? h : t == GD_AIR ? 0 : t == GD_GAS ? 1 : stratum_hardness((unsigned)y);
}
unsigned gd_hardness(const GdRun *r, int x, int y) { return hardness_at(r, x, y); }
unsigned gd_hits_left(const GdGame *g, int x, int y) {
    const GdRun *r = &g->run;
    unsigned need = gd_hits_for(&g->profile, hardness_at(r, x, y));
    if (need && r->crack_hits && r->crack_x == x && r->crack_y == y) need -= r->crack_hits < need ? r->crack_hits : need - 1;
    return need;
}
static void note_depth(GdGame *g) {
    if (g->run.y > g->run.deepest) g->run.deepest = g->run.y;
}
bool gd_dive(GdGame *g, unsigned stop) {
    if (g->run.active || !gd_stop_owned(&g->profile, stop)) return false;
    GdRun *r = &g->run;
    memset(r, 0, sizeof(*r));
    uint32_t seed = g->profile.next_seed++ * 0x85ebca6bu ^ (g->profile.dives + 1) * 0xc2b2ae35u;
    r->seed = seed ? seed : 1;
    generate(r, r->seed);
    r->active = 1;
    r->x = 3;
    r->y = (uint16_t)(stop * GD_STOP_SPACING);
    r->start_depth = r->deepest = r->y;
    if (stop) { /* the elevator cage: an air pocket on solid ground */
        *cell(r, r->x, r->y) = GD_AIR;
        GdTile below = gd_tile(r, r->x, r->y + 1);
        if (below == GD_AIR || below == GD_LAVA) *cell(r, r->x, r->y + 1) = (uint8_t)stratum(r->y + 1);
    }
    r->energy = r->energy_max = (uint16_t)gd_energy_max(&g->profile);
    r->bag_cap = (uint16_t)gd_bag_capacity(&g->profile);
    r->facing = 1;
    ++g->profile.dives;
    memset(&g->report, 0, sizeof(g->report));
    return true;
}
uint64_t gd_ore_value(const GdProfile *p, unsigned kind) {
    if (kind >= GD_ORE_KINDS) return 0;
    uint64_t base = tiles[GD_ORE_FIRST + kind].value;
    return base + base * gd_charm_percent(p) / 100;
}
uint64_t gd_bag_value(const GdGame *g) {
    uint64_t v = g->run.chest_gold;
    for (unsigned i = 0; i < GD_ORE_KINDS; ++i) v += g->run.bag[i] * gd_ore_value(&g->profile, i);
    return v;
}
static uint32_t chest_value(unsigned y) { return y >= GD_ROWS - 2 ? 2500u : 10u + y * 3u / 2u; }
static void spend(GdRun *r, unsigned cost) { r->energy = (uint16_t)(r->energy > cost ? r->energy - cost : 0); }

/* Breaks the tile at (x, y) and collects what it held. */
static GdEvent take(GdGame *g, int x, int y) {
    GdRun *r = &g->run;
    GdTile t = gd_tile(r, x, y);
    GdEvent e = {GD_EV_DIG, 0, (uint8_t)x, (uint8_t)(y - r->y)};
    *cell(r, x, y) = GD_AIR;
    if (r->crack_x == x && r->crack_y == y) r->crack_hits = 0;
    ++r->digs;
    if (t >= GD_COAL && t <= GD_DIAMOND) {
        if (gd_bag_used(r) < r->bag_cap) {
            ++r->bag[t - GD_ORE_FIRST];
            e.kind = GD_EV_ORE;
        } else
            e.kind = GD_EV_BAG_FULL;
        e.value = t - GD_ORE_FIRST;
    } else if (t == GD_CHEST) {
        e.kind = GD_EV_CHEST;
        e.value = chest_value((unsigned)y);
        r->chest_gold += e.value;
    } else if (t == GD_GAS) {
        e.kind = GD_EV_GAS;
        e.value = 8u + (unsigned)y / 25u;
        spend(r, e.value);
    }
    return e;
}
/* Gravity after every step: the miner drops through air; lava swallows them. */
static GdEvent settle(GdGame *g, GdEvent e) {
    GdRun *r = &g->run;
    unsigned fallen = 0;
    while (gd_tile(r, r->x, r->y + 1) == GD_AIR) {
        ++r->y;
        ++fallen;
    }
    note_depth(g);
    if (fallen && gd_tile(r, r->x, r->y + 1) == GD_LAVA) {
        gd_end_dive(g, GD_END_BURNED);
        return (GdEvent){GD_EV_BURNED, fallen, r->x, 0};
    }
    if (fallen > 3) spend(r, (fallen - 3) * 2);
    if (fallen && e.kind == GD_EV_WALK) e = (GdEvent){GD_EV_FALL, fallen, r->x, 0};
    if (!r->energy) {
        gd_end_dive(g, GD_END_FAINT);
        return (GdEvent){GD_EV_FAINT, 0, r->x, 0};
    }
    return e;
}
GdEvent gd_move(GdGame *g, GdDir dir) {
    GdRun *r = &g->run;
    GdEvent none = {GD_EV_NONE, 0, r->x, 0};
    if (!r->active) return none;
    int nx = r->x + (dir == GD_LEFT ? -1 : dir == GD_RIGHT ? 1 : 0), ny = r->y + (dir == GD_DOWN ? 1 : 0);
    if (dir != GD_DOWN) r->facing = dir == GD_RIGHT;
    GdTile t = gd_tile(r, nx, ny);
    if (nx < 0 || nx >= GD_COLS || t == GD_BEDROCK || t == GD_LAVA ||
        (dir != GD_DOWN && gd_tile(r, nx, ny + 1) == GD_LAVA))
        return (GdEvent){GD_EV_BLOCKED, t == GD_LAVA || gd_tile(r, nx, ny + 1) == GD_LAVA, r->x, 0};
    GdEvent e = {GD_EV_WALK, 0, (uint8_t)nx, 0};
    if (t != GD_AIR) {
        /* One swing, one energy. Hard rock needs several swings; the miner
           stays put until the last one breaks it. */
        unsigned left = gd_hits_left(g, nx, ny);
        spend(r, 1);
        if (left > 1) {
            if (r->crack_x != nx || r->crack_y != ny) r->crack_hits = 0;
            r->crack_x = (uint8_t)nx;
            r->crack_y = (uint16_t)ny;
            ++r->crack_hits;
            if (!r->energy) {
                gd_end_dive(g, GD_END_FAINT);
                return (GdEvent){GD_EV_FAINT, 0, r->x, 0};
            }
            return (GdEvent){GD_EV_HIT, left - 1, (uint8_t)nx, (uint8_t)(ny - r->y)};
        }
        r->crack_hits = 0;
        e = take(g, nx, ny);
        e.y_offset = (uint8_t)(ny - r->y);
    }
    r->x = (uint8_t)nx;
    r->y = (uint16_t)ny;
    e.y_offset = 0;
    return settle(g, e);
}
GdEvent gd_blast(GdGame *g) {
    GdRun *r = &g->run;
    if (!r->active || !g->profile.level[GD_DYNAMITE]) return (GdEvent){GD_EV_NONE, 0, r->x, 0};
    --g->profile.level[GD_DYNAMITE];
    GdEvent e = {GD_EV_BLAST, 0, r->x, 0};
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            int x = r->x + dx, y = r->y + dy;
            GdTile t = gd_tile(r, x, y);
            if ((dx || dy) && y > 0 && t != GD_AIR && t != GD_BEDROCK && t != GD_LAVA && x >= 0 && x < GD_COLS) {
                GdEvent got = take(g, x, y);
                if (got.kind == GD_EV_ORE || got.kind == GD_EV_CHEST) ++e.value;
            }
        }
    return settle(g, e);
}
void gd_end_dive(GdGame *g, GdEnd why) {
    GdRun *r = &g->run;
    if (!r->active) return;
    GdReport *rep = &g->report;
    memset(rep, 0, sizeof(*rep));
    rep->end = why;
    for (unsigned i = 0; i < GD_ORE_KINDS; ++i) {
        unsigned keep = why == GD_END_ROPE ? r->bag[i] : why == GD_END_FAINT ? r->bag[i] / 2 : 0;
        rep->lost = (uint16_t)(rep->lost + r->bag[i] - keep);
        rep->ore[i] = (uint16_t)keep;
        rep->ore_gold += keep * gd_ore_value(&g->profile, i);
    }
    rep->chest_gold = why == GD_END_ROPE ? r->chest_gold : why == GD_END_FAINT ? r->chest_gold / 2 : 0;
    rep->total = rep->ore_gold + rep->chest_gold;
    rep->deepest = r->deepest;
    rep->new_best = r->deepest > g->profile.best_depth;
    if (rep->new_best) g->profile.best_depth = r->deepest;
    g->profile.gold += rep->total;
    g->profile.lifetime_gold += rep->total;
    r->active = 0;
}

/* ---- saves ---------------------------------------------------------------- */
static char save_version[GD_SAVE_VERSION_BYTES + 1] = "dev";
void gd_save_set_version(const char *version) {
    memset(save_version, 0, sizeof(save_version));
    strncpy(save_version, version ? version : "", GD_SAVE_VERSION_BYTES);
}
static uint32_t crc32(const uint8_t *d, size_t n) {
    uint32_t c = 0xffffffffu;
    while (n--) {
        c ^= *d++;
        for (unsigned k = 0; k < 8; ++k) c = c & 1 ? (c >> 1) ^ 0xedb88320u : c >> 1;
    }
    return ~c;
}
typedef struct {
    uint8_t *p;
    size_t at, cap;
    bool ok;
} Writer;
static void w8(Writer *w, uint64_t v, unsigned bytes) {
    if (w->at + bytes > w->cap) {
        w->ok = false;
        return;
    }
    for (unsigned i = 0; i < bytes; ++i) w->p[w->at++] = (uint8_t)(v >> (8 * i));
}
typedef struct {
    const uint8_t *p;
    size_t at, size;
    bool ok;
} Reader;
static uint64_t r8(Reader *r, unsigned bytes) {
    uint64_t v = 0;
    if (r->at + bytes > r->size) {
        r->ok = false;
        return 0;
    }
    for (unsigned i = 0; i < bytes; ++i) v |= (uint64_t)r->p[r->at++] << (8 * i);
    return v;
}
#define HEADER (4 + 1 + GD_SAVE_VERSION_BYTES)
size_t gd_save_encode(const GdGame *g, uint8_t *out, size_t capacity) {
    Writer w = {out, 0, capacity, true};
    const GdProfile *p = &g->profile;
    const GdRun *r = &g->run;
    const GdReport *rep = &g->report;
    w8(&w, 0x56534447u, 4); /* "GDSV" */
    w8(&w, GD_SCHEMA, 1);
    for (unsigned i = 0; i < GD_SAVE_VERSION_BYTES; ++i) w8(&w, (uint8_t)save_version[i], 1);
    w8(&w, p->gold, 8);
    w8(&w, p->lifetime_gold, 8);
    for (unsigned i = 0; i < GD_UPGRADE_COUNT; ++i) w8(&w, p->level[i], 1);
    w8(&w, p->best_depth, 2);
    w8(&w, p->stops, 1);
    w8(&w, p->dives, 4);
    w8(&w, p->next_seed, 4);
    w8(&w, r->active, 1);
    if (r->active) {
        w8(&w, r->seed, 4);
        w8(&w, r->x, 1);
        w8(&w, r->y, 2);
        w8(&w, r->energy, 2);
        w8(&w, r->energy_max, 2);
        for (unsigned i = 0; i < GD_ORE_KINDS; ++i) w8(&w, r->bag[i], 2);
        w8(&w, r->bag_cap, 2);
        w8(&w, r->chest_gold, 4);
        w8(&w, r->start_depth, 2);
        w8(&w, r->deepest, 2);
        w8(&w, r->facing, 1);
        w8(&w, r->digs, 4);
        if (w.ok && w.at + sizeof(r->tiles) <= w.cap) {
            memcpy(out + w.at, r->tiles, sizeof(r->tiles));
            w.at += sizeof(r->tiles);
        } else
            w.ok = false;
    }
    w8(&w, rep->end, 1);
    for (unsigned i = 0; i < GD_ORE_KINDS; ++i) w8(&w, rep->ore[i], 2);
    w8(&w, rep->lost, 2);
    w8(&w, rep->ore_gold, 8);
    w8(&w, rep->chest_gold, 8);
    w8(&w, rep->total, 8);
    w8(&w, rep->deepest, 2);
    w8(&w, rep->new_best, 1);
    if (!w.ok) return 0;
    uint32_t crc = crc32(out, w.at);
    w8(&w, crc, 4);
    return w.ok ? w.at : 0;
}
static bool header_ok(const uint8_t *d, size_t size) {
    return size >= HEADER + 4 && d[0] == 'G' && d[1] == 'D' && d[2] == 'S' && d[3] == 'V';
}
unsigned gd_save_newer(const uint8_t *d, size_t size) {
    return header_ok(d, size) && d[4] > GD_SCHEMA ? d[4] : 0;
}
bool gd_save_decode(GdGame *g, const uint8_t *d, size_t size) {
    if (!header_ok(d, size) || (d[4] != GD_SCHEMA && d[4] != 1)) return false;
    uint32_t crc = (uint32_t)d[size - 4] | (uint32_t)d[size - 3] << 8 | (uint32_t)d[size - 2] << 16 |
                   (uint32_t)d[size - 1] << 24;
    if (crc32(d, size - 4) != crc) return false;
    GdGame *n = g; /* decode into a scratch copy so a bad save leaves g alone */
    static GdGame scratch;
    memset(&scratch, 0, sizeof(scratch));
    Reader r = {d, HEADER, size - 4, true};
    GdProfile *p = &scratch.profile;
    GdRun *run = &scratch.run;
    GdReport *rep = &scratch.report;
    p->gold = r8(&r, 8);
    p->lifetime_gold = r8(&r, 8);
    for (unsigned i = 0; i < GD_UPGRADE_COUNT; ++i) {
        if (d[4] == 1 && i == GD_ENERGY) continue; /* v1: lantern covered energy too */
        p->level[i] = (uint8_t)r8(&r, 1);
        if (p->level[i] > upgrade_max[i]) return false;
    }
    if (d[4] == 1) p->level[GD_ENERGY] = p->level[GD_LANTERN];
    p->best_depth = (uint16_t)r8(&r, 2);
    p->stops = (uint8_t)(r8(&r, 1) | 1u);
    p->dives = (uint32_t)r8(&r, 4);
    p->next_seed = (uint32_t)r8(&r, 4);
    run->active = (uint8_t)r8(&r, 1);
    if (run->active > 1 || p->best_depth >= GD_ROWS) return false;
    if (run->active) {
        run->seed = (uint32_t)r8(&r, 4);
        run->x = (uint8_t)r8(&r, 1);
        run->y = (uint16_t)r8(&r, 2);
        run->energy = (uint16_t)r8(&r, 2);
        run->energy_max = (uint16_t)r8(&r, 2);
        for (unsigned i = 0; i < GD_ORE_KINDS; ++i) run->bag[i] = (uint16_t)r8(&r, 2);
        run->bag_cap = (uint16_t)r8(&r, 2);
        run->chest_gold = (uint32_t)r8(&r, 4);
        run->start_depth = (uint16_t)r8(&r, 2);
        run->deepest = (uint16_t)r8(&r, 2);
        run->facing = (uint8_t)r8(&r, 1);
        run->digs = (uint32_t)r8(&r, 4);
        if (!r.ok || r.at + sizeof(run->tiles) > r.size) return false;
        memcpy(run->tiles, d + r.at, sizeof(run->tiles));
        r.at += sizeof(run->tiles);
        if (run->x >= GD_COLS || run->y >= GD_ROWS || run->energy > run->energy_max ||
            gd_bag_used(run) > run->bag_cap || run->deepest >= GD_ROWS)
            return false;
        for (unsigned i = 0; i < sizeof(run->tiles); ++i)
            if (run->tiles[i] >= GD_TILE_COUNT) return false;
    }
    rep->end = (GdEnd)r8(&r, 1);
    for (unsigned i = 0; i < GD_ORE_KINDS; ++i) rep->ore[i] = (uint16_t)r8(&r, 2);
    rep->lost = (uint16_t)r8(&r, 2);
    rep->ore_gold = r8(&r, 8);
    rep->chest_gold = r8(&r, 8);
    rep->total = r8(&r, 8);
    rep->deepest = (uint16_t)r8(&r, 2);
    rep->new_best = r8(&r, 1) != 0;
    if (!r.ok || r.at != r.size || rep->end > GD_END_BURNED) return false;
    *n = scratch;
    return true;
}
