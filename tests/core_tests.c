/* Host tests for the portable rules in core/. */
#include "digger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c)                                                         \
    do {                                                                 \
        if (!(c)) {                                                      \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); \
            ++failures;                                                  \
        }                                                                \
    } while (0)

static GdGame g, h;
static uint8_t buf[GD_SAVE_BYTES];

static void test_new_game(void) {
    gd_init(&g);
    CHECK(g.profile.gold == 0);
    CHECK(gd_stop_owned(&g.profile, 0));
    CHECK(!gd_stop_owned(&g.profile, 1));
    CHECK(gd_hits_for(&g.profile, 1) == 1 && gd_hits_for(&g.profile, 2) == 2 && gd_hits_for(&g.profile, 3) == 3);
    CHECK(gd_dive(&g, 0));
    CHECK(!gd_dive(&g, 0)); /* already diving */
    CHECK(g.run.y == 0 && g.run.x == 3);
    CHECK(gd_tile(&g.run, 0, 0) == GD_AIR);
    CHECK(gd_tile(&g.run, 0, GD_ROWS - 1) == GD_BEDROCK);
    CHECK(gd_tile(&g.run, -1, 5) == GD_BEDROCK);
}
static void test_deterministic(void) {
    gd_init(&g);
    gd_init(&h);
    gd_dive(&g, 0);
    gd_dive(&h, 0);
    CHECK(!memcmp(g.run.tiles, h.run.tiles, sizeof(g.run.tiles)));
    gd_end_dive(&g, GD_END_ROPE);
    gd_dive(&g, 0);
    CHECK(memcmp(g.run.tiles, h.run.tiles, sizeof(g.run.tiles)) != 0); /* each dive is a new mine */
}
static void test_dig_and_bank(void) {
    gd_init(&g);
    gd_dive(&g, 0);
    /* Put copper right below and solid dirt under it. */
    g.run.tiles[1 * GD_COLS + 3] = GD_COPPER;
    g.run.tiles[2 * GD_COLS + 3] = GD_DIRT;
    unsigned energy = g.run.energy;
    GdEvent e = gd_move(&g, GD_DOWN);
    CHECK(e.kind == GD_EV_ORE && e.value == GD_COPPER - GD_ORE_FIRST);
    CHECK(g.run.y == 1);
    CHECK(g.run.energy == energy - 1);
    CHECK(g.run.bag[GD_COPPER - GD_ORE_FIRST] == 1);
    gd_end_dive(&g, GD_END_ROPE);
    CHECK(!g.run.active);
    CHECK(g.profile.gold == 5);
    CHECK(g.report.total == 5 && g.report.deepest == 1 && g.report.new_best);
    CHECK(g.profile.best_depth == 1);
}
static void test_hardness_and_lava(void) {
    gd_init(&g);
    gd_dive(&g, 0);
    g.run.tiles[1 * GD_COLS + 3] = GD_GRANITE; /* hardness 4: four swings */
    g.run.tiles[2 * GD_COLS + 3] = GD_DIRT;
    unsigned energy = g.run.energy;
    GdEvent e;
    for (unsigned left = 3; left > 0; --left) {
        e = gd_move(&g, GD_DOWN);
        CHECK(e.kind == GD_EV_HIT && e.value == left && g.run.y == 0);
        CHECK(gd_hits_left(&g, 3, 1) == left);
    }
    e = gd_move(&g, GD_DOWN);
    CHECK(e.kind == GD_EV_DIG && g.run.y == 1 && g.run.energy == energy - 4);
    gd_end_dive(&g, GD_END_ROPE);
    gd_dive(&g, 0);
    g.run.tiles[1 * GD_COLS + 3] = GD_LAVA;
    CHECK(gd_move(&g, GD_DOWN).kind == GD_EV_BLOCKED);
    /* Lava under the next column: walking there is refused. */
    g.run.tiles[0 * GD_COLS + 4] = GD_AIR;
    g.run.tiles[1 * GD_COLS + 4] = GD_LAVA;
    CHECK(gd_move(&g, GD_RIGHT).kind == GD_EV_BLOCKED && g.run.x == 3);
    /* Falling down a shaft into lava burns the bag. */
    g.run.bag[0] = 4;
    g.run.tiles[0 * GD_COLS + 2] = GD_AIR;
    for (unsigned y = 1; y < 6; ++y) g.run.tiles[y * GD_COLS + 2] = GD_AIR;
    g.run.tiles[6 * GD_COLS + 2] = GD_LAVA;
    e = gd_move(&g, GD_LEFT);
    CHECK(e.kind == GD_EV_BURNED);
    CHECK(!g.run.active && g.report.end == GD_END_BURNED && g.report.total == 0 && g.report.lost == 4);
}
static void test_faint(void) {
    gd_init(&g);
    gd_dive(&g, 0);
    g.run.energy = 1;
    g.run.bag[GD_GOLD - GD_ORE_FIRST] = 3;
    g.run.tiles[1 * GD_COLS + 3] = GD_DIRT;
    g.run.tiles[2 * GD_COLS + 3] = GD_DIRT;
    GdEvent e = gd_move(&g, GD_DOWN);
    CHECK(e.kind == GD_EV_FAINT);
    CHECK(g.report.ore[GD_GOLD - GD_ORE_FIRST] == 1 && g.report.lost == 2);
    CHECK(g.profile.gold == 30);
}
static void test_shop_and_stops(void) {
    gd_init(&g);
    CHECK(!gd_buy_upgrade(&g, GD_PICK));
    g.profile.gold = 1000;
    CHECK(gd_upgrade_price(&g.profile, GD_PICK) == 120);
    CHECK(gd_buy_upgrade(&g, GD_PICK));
    CHECK(g.profile.gold == 880 && gd_hits_for(&g.profile, 2) == 1 && gd_hits_for(&g.profile, 3) == 2);
    CHECK(!gd_buy_stop(&g, 1)); /* not reached yet */
    g.profile.best_depth = 60;
    CHECK(gd_buy_stop(&g, 1));
    CHECK(g.profile.gold == 730);
    CHECK(gd_dive(&g, 1) && g.run.y == 50);
    CHECK(gd_tile(&g.run, 3, 51) != GD_AIR && gd_tile(&g.run, 3, 51) != GD_LAVA);
    CHECK(!gd_buy_upgrade(&g, GD_LANTERN)); /* not while diving */
    CHECK(gd_buy_upgrade(&g, GD_DYNAMITE));
    g.run.tiles[51 * GD_COLS + 3] = GD_SILVER;
    GdEvent e = gd_blast(&g);
    CHECK(e.kind == GD_EV_BLAST || e.kind == GD_EV_FALL);
    CHECK(g.profile.level[GD_DYNAMITE] == 0);
    CHECK(g.run.bag[GD_SILVER - GD_ORE_FIRST] >= 1);
    for (unsigned u = 0; u < GD_UPGRADE_COUNT; ++u) {
        g.profile.level[u] = (uint8_t)gd_upgrade_max(u);
        CHECK(gd_upgrade_price(&g.profile, u) == 0);
    }
}
static void test_save_roundtrip(void) {
    gd_init(&g);
    g.profile.gold = 123456789012ull;
    g.profile.level[GD_BAG] = 3;
    gd_save_set_version("0.1.0");
    gd_dive(&g, 0);
    g.run.bag[2] = 2;
    size_t n = gd_save_encode(&g, buf, sizeof(buf));
    CHECK(n > 3200 && n <= GD_SAVE_BYTES);
    gd_init(&h);
    CHECK(gd_save_decode(&h, buf, n));
    CHECK(h.profile.gold == g.profile.gold && h.profile.level[GD_BAG] == 3);
    CHECK(h.run.active && !memcmp(h.run.tiles, g.run.tiles, sizeof(h.run.tiles)) && h.run.bag[2] == 2);
    buf[40] ^= 1;
    gd_init(&h);
    CHECK(!gd_save_decode(&h, buf, n));
    CHECK(h.profile.gold == 0); /* untouched on failure */
    buf[40] ^= 1;
    buf[4] = GD_SCHEMA + 1;
    CHECK(gd_save_newer(buf, n) == GD_SCHEMA + 1);
    CHECK(!gd_save_decode(&h, buf, n));
    gd_end_dive(&g, GD_END_ROPE);
    n = gd_save_encode(&g, buf, sizeof(buf));
    CHECK(n > 0 && n < 200);
    CHECK(gd_save_decode(&h, buf, n) && !h.run.active && h.report.total == g.report.total);
}
static void test_generation_reachable(void) {
    /* Every mine has ore near the top and something worth finding deep down. */
    gd_init(&g);
    unsigned shallow = 0, deep = 0;
    gd_dive(&g, 0);
    for (unsigned y = 1; y < 40; ++y)
        for (unsigned x = 0; x < GD_COLS; ++x) {
            GdTile t = gd_tile(&g.run, (int)x, (int)y);
            shallow += t >= GD_COAL && t <= GD_SILVER;
        }
    for (unsigned y = 250; y < GD_ROWS; ++y)
        for (unsigned x = 0; x < GD_COLS; ++x) {
            GdTile t = gd_tile(&g.run, (int)x, (int)y);
            deep += t == GD_RUBY || t == GD_DIAMOND || t == GD_CHEST;
        }
    CHECK(shallow >= 8);
    CHECK(deep >= 5);
}
static void test_simulated_dives(void) {
    /* A naive bot that always digs down and ropes out when the bag fills
       makes progress and never corrupts state. */
    gd_init(&g);
    for (unsigned dive = 0; dive < 40; ++dive) {
        CHECK(gd_dive(&g, 0));
        for (unsigned step = 0; step < 2000 && g.run.active; ++step) {
            GdEvent e = gd_move(&g, GD_DOWN);
            if (e.kind == GD_EV_BLOCKED)
                e = gd_move(&g, step & 1 ? GD_LEFT : GD_RIGHT);
            if (g.run.active && (gd_bag_used(&g.run) >= g.run.bag_cap || g.run.energy < 4))
                gd_end_dive(&g, GD_END_ROPE);
            if (step == 1999 && g.run.active) gd_end_dive(&g, GD_END_ROPE);
        }
        for (unsigned u = 0; u < GD_DYNAMITE; ++u) gd_buy_upgrade(&g, (GdUpgrade)u);
    }
    CHECK(g.profile.lifetime_gold > 200);
    CHECK(g.profile.best_depth > 20);
    printf("bot: %llu gold lifetime, best %u m, pick %u lantern %u bag %u\n",
           (unsigned long long)g.profile.lifetime_gold, g.profile.best_depth, g.profile.level[GD_PICK],
           g.profile.level[GD_LANTERN], g.profile.level[GD_BAG]);
}

int main(void) {
    test_new_game();
    test_deterministic();
    test_dig_and_bank();
    test_hardness_and_lava();
    test_faint();
    test_shop_and_stops();
    test_save_roundtrip();
    test_generation_reachable();
    test_simulated_dives();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    puts("core_tests: all passed");
    return 0;
}
