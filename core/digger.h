/* Gold Digger: portable game rules. No LVGL, ESP-IDF or platform code here;
   the Passport UI and every host port drive the game only through this API. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GD_COLS               8
#define GD_ROWS               400 /* one row = one metre; row 0 is the surface */
#define GD_STOP_SPACING       50  /* elevator stops every 50 m */
#define GD_STOP_COUNT         (GD_ROWS / GD_STOP_SPACING)
#define GD_SAVE_VERSION_BYTES 16
#define GD_SAVE_BYTES         4096
#define GD_SCHEMA             2
#define GD_DYNAMITE_MAX       9

typedef enum {
    GD_AIR,
    GD_DIRT,
    GD_CLAY,
    GD_STONE,
    GD_GRANITE,
    GD_BASALT,
    GD_OBSIDIAN,
    GD_BEDROCK, /* the floor of the mine: never breaks */
    GD_COAL,
    GD_COPPER,
    GD_SILVER,
    GD_GOLD,
    GD_RUBY,
    GD_DIAMOND,
    GD_CHEST,
    GD_GAS,
    GD_LAVA,
    GD_TILE_COUNT
} GdTile;

/* Ores carried in the bag (index = tile - GD_COAL). */
enum { GD_ORE_FIRST = GD_COAL, GD_ORE_KINDS = GD_DIAMOND - GD_COAL + 1 };

typedef enum { GD_PICK, GD_LANTERN, GD_ENERGY, GD_BAG, GD_CHARM, GD_DYNAMITE, GD_UPGRADE_COUNT } GdUpgrade;
typedef enum { GD_LEFT, GD_RIGHT, GD_DOWN } GdDir;

/* What the last action did, for sounds, floating text and messages. */
typedef enum {
    GD_EV_NONE,
    GD_EV_WALK,
    GD_EV_DIG,
    GD_EV_ORE,      /* value = ore kind */
    GD_EV_BAG_FULL, /* ore dug but lost */
    GD_EV_CHEST,    /* value = gold */
    GD_EV_GAS,      /* value = energy lost */
    GD_EV_TOO_HARD, /* unused since hit-count digging; kept so event numbers stay stable */
    GD_EV_HIT,      /* a swing that cracked the tile; value = hits still needed */
    GD_EV_BLOCKED, /* bedrock, lava beside or below, or the edge */
    GD_EV_FALL,    /* value = metres fallen */
    GD_EV_FAINT,   /* out of energy: dive ends, half the bag is lost */
    GD_EV_BURNED,  /* fell into lava: dive ends, the whole bag is lost */
    GD_EV_BLAST,   /* dynamite */
    GD_EV_NO_ENERGY
} GdEventKind;
typedef struct {
    GdEventKind kind;
    uint32_t value;
    uint8_t x, y_offset; /* where (column, rows below the player) for floating text */
} GdEvent;

typedef enum { GD_END_NONE, GD_END_ROPE, GD_END_FAINT, GD_END_BURNED } GdEnd;

typedef struct {
    uint8_t active;
    uint32_t seed;
    uint8_t x;
    uint16_t y;
    uint16_t energy, energy_max;
    uint16_t bag[GD_ORE_KINDS];
    uint16_t bag_cap;
    uint32_t chest_gold;
    uint16_t start_depth, deepest;
    uint8_t facing; /* 0 left, 1 right */
    uint32_t digs;
    uint8_t tiles[GD_COLS * GD_ROWS];
    /* The tile being picked at and the hits it has taken. Not saved: a reload
       leaves it uncracked. */
    uint8_t crack_x, crack_hits;
    uint16_t crack_y;
} GdRun;

typedef struct {
    GdEnd end;
    uint16_t ore[GD_ORE_KINDS]; /* delivered (after losses) */
    uint16_t lost;              /* ore pieces lost */
    uint64_t ore_gold, chest_gold, total;
    uint16_t deepest;
    bool new_best;
} GdReport;

typedef struct {
    uint64_t gold, lifetime_gold;
    uint8_t level[GD_UPGRADE_COUNT]; /* GD_DYNAMITE = sticks owned */
    uint16_t best_depth;
    uint8_t stops; /* bit k: elevator stop k (k*50 m) bought; bit 0 = surface, always set */
    uint32_t dives, next_seed;
} GdProfile;

typedef struct {
    GdProfile profile;
    GdRun run;
    GdReport report;
} GdGame;

typedef struct {
    const char *name;
    uint8_t hardness; /* hits with the starting pickaxe; 0 = no digging needed, 255 = unbreakable */
    uint16_t value;   /* gold per piece for ores */
    uint32_t color;   /* 0xRRGGBB base colour for renderers */
} GdTileInfo;
const GdTileInfo *gd_tile_info(GdTile tile);
const char *gd_upgrade_name(GdUpgrade upgrade);
const char *gd_upgrade_blurb(GdUpgrade upgrade);
unsigned gd_upgrade_max(GdUpgrade upgrade);
/* Price of the next level (or one stick of dynamite); 0 when maxed. */
uint64_t gd_upgrade_price(const GdProfile *p, GdUpgrade upgrade);
bool gd_buy_upgrade(GdGame *g, GdUpgrade upgrade);

unsigned gd_pick_power(const GdProfile *p); /* hits saved per tile, never below one */
/* Swings needed to break a tile of this hardness: hardness - pickaxe level, at least 1. */
unsigned gd_hits_for(const GdProfile *p, unsigned hardness);
unsigned gd_energy_max(const GdProfile *p);
unsigned gd_bag_capacity(const GdProfile *p);
unsigned gd_charm_percent(const GdProfile *p); /* bonus % on ore value */
unsigned gd_light_radius(const GdProfile *p);  /* tiles seen clearly around the miner */
bool gd_sees_gas(const GdProfile *p);

/* Elevator stops: stop k starts a dive at k*50 m. */
bool gd_stop_reached(const GdProfile *p, unsigned stop); /* best depth deep enough to buy it */
bool gd_stop_owned(const GdProfile *p, unsigned stop);
uint64_t gd_stop_price(unsigned stop);
bool gd_buy_stop(GdGame *g, unsigned stop);

void gd_init(GdGame *g);
void gd_new_profile(GdGame *g);
/* Starts a dive from an owned stop. False if the stop is not owned or a dive is active. */
bool gd_dive(GdGame *g, unsigned stop);
GdTile gd_tile(const GdRun *r, int x, int y); /* out of range: bedrock */
GdTile gd_stratum(unsigned y);                     /* the host rock at a depth */
unsigned gd_hardness(const GdRun *r, int x, int y); /* 0 for air, 255 unbreakable */
/* Swings still needed to break (x, y) for this game; 0 for air or unbreakable. */
unsigned gd_hits_left(const GdGame *g, int x, int y);
unsigned gd_bag_used(const GdRun *r);
GdEvent gd_move(GdGame *g, GdDir dir);
GdEvent gd_blast(GdGame *g); /* uses one stick: clears the 3x3 around the miner and collects ores */
/* Ends the dive (rope = voluntary), banks the haul and fills g->report. */
void gd_end_dive(GdGame *g, GdEnd why);
uint64_t gd_ore_value(const GdProfile *p, unsigned ore_kind);
uint64_t gd_bag_value(const GdGame *g);

/* Saves: "GDSV", schema, version string, payload, CRC-32. */
void gd_save_set_version(const char *version);
size_t gd_save_encode(const GdGame *g, uint8_t *out, size_t capacity);
bool gd_save_decode(GdGame *g, const uint8_t *data, size_t size);
unsigned gd_save_newer(const uint8_t *data, size_t size); /* schema if newer than ours, else 0 */
