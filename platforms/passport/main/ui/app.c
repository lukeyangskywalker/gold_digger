/* Gold Digger UI state, input dispatch, Passport power policy and the main loop. */
#include "ui.h"

GdGame game;
Page page = HOME;
unsigned selected = STATION_MINE;
uint8_t preferences[PREF_COUNT] = {50, 80, 80, 1};
const unsigned sleep_seconds[5] = {30, 60, 120, 300, 0};
uint32_t clock_ms;
bool save_enabled = true, pointer_mode, battery_charging;
int battery_percent = -1;
const char *TAG = "golddigger";
char notice[64];
uint32_t notice_until;
Floater floaters[FLOATERS];
uint32_t shake_until;

static QueueHandle_t inputs;
static bool redraw_wanted, screen_dimmed, screen_dark, wake_guard, low_energy_warned;
static uint32_t last_input_ms, wake_released_ms, repeat_next_ms, usb_seen_ms, last_mine_save_ms;
static int repeat_button = -1;
static uint32_t usb_frame;
static int64_t loop_previous;

const char *firmware_version(void) { return esp_app_get_description()->version; }
bool save_now(void) {
    if (!save_enabled) return false;
    esp_err_t err = storage_save(&game);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Save failed: %s", esp_err_to_name(err));
        show_notice("Saving failed");
        return false;
    }
    return true;
}
void show_notice(const char *text) {
    snprintf(notice, sizeof(notice), "%s", text);
    notice_until = clock_ms + 1800;
}
static void floater(const char *text, uint32_t color, unsigned x, unsigned row) {
    unsigned slot = 0;
    for (unsigned i = 1; i < FLOATERS; ++i)
        if (floaters[i].start < floaters[slot].start) slot = i;
    Floater *f = &floaters[slot];
    snprintf(f->text, sizeof(f->text), "%s", text);
    f->color = color;
    f->start = clock_ms ? clock_ms : 1;
    f->x = (uint8_t)x;
    f->row = (uint16_t)row;
}
unsigned page_rows(Page p) {
    switch (p) {
        case HOME: return STATION_COUNT;
        case PAUSE: return PAUSE_ROWS;
        case SHOP: return SHOP_ROWS;
        case ELEVATOR: return GD_STOP_COUNT + 1;
        case SETTINGS: return SETTINGS_ROWS;
        case RESET_CONFIRM: return 2;
        default: return 1;
    }
}
static void go(Page p, unsigned row) {
    page = p;
    selected = row;
}
static void save_preferences(void) {
    if (save_enabled) storage_preferences_save(preferences);
}

/* ---- mining --------------------------------------------------------------- */
static void finish_dive(void) {
    go(REPORT, 0);
    save_now();
}
static void react(GdEvent e) {
    char text[64];
    const GdRun *r = &game.run;
    switch (e.kind) {
        case GD_EV_HIT:
            game_audio_effect(GD_SOUND_DIG);
            shake_until = clock_ms + 50;
            break;
        case GD_EV_DIG:
            game_audio_effect(GD_SOUND_DIG);
            shake_until = clock_ms + 80;
            break;
        case GD_EV_ORE:
            game_audio_effect(GD_SOUND_ORE);
            snprintf(text, sizeof(text), "+%s", gd_tile_info(GD_ORE_FIRST + e.value)->name);
            floater(text, gd_tile_info(GD_ORE_FIRST + e.value)->color, r->x, r->y);
            break;
        case GD_EV_BAG_FULL:
            game_audio_effect(GD_SOUND_DENY);
            show_notice("Bag full! Ore lost");
            break;
        case GD_EV_CHEST:
            game_audio_effect(GD_SOUND_CHEST);
            snprintf(text, sizeof(text), "+%lu gold", (unsigned long)e.value);
            floater(text, 0xffde6e, r->x, r->y);
            break;
        case GD_EV_GAS:
            game_audio_effect(GD_SOUND_GAS);
            snprintf(text, sizeof(text), "Gas! -%lu energy", (unsigned long)e.value);
            show_notice(text);
            floater("Gas!", 0x9fd36a, r->x, r->y);
            break;
        case GD_EV_BLOCKED:
            game_audio_effect(GD_SOUND_DENY);
            if (e.value) show_notice("Lava! Too hot to go there");
            break;
        case GD_EV_FALL:
            if (e.value > 3) {
                game_audio_effect(GD_SOUND_HURT);
                snprintf(text, sizeof(text), "-%lu", (unsigned long)(e.value - 3) * 2);
                floater(text, 0xff7a6a, r->x, r->y);
            }
            break;
        case GD_EV_BLAST:
            game_audio_effect(GD_SOUND_BLAST);
            shake_until = clock_ms + 350;
            break;
        case GD_EV_FAINT:
        case GD_EV_BURNED:
            game_audio_effect(GD_SOUND_HURT);
            finish_dive();
            return;
        default: break;
    }
    if (!game.run.active) {
        finish_dive();
        return;
    }
    if (!low_energy_warned && r->energy * 5 <= r->energy_max) {
        low_energy_warned = true;
        show_notice("Lantern low! Rope up soon");
    }
}
static void start_dive(unsigned stop) {
    if (!gd_dive(&game, stop)) {
        game_audio_effect(GD_SOUND_DENY);
        return;
    }
    low_energy_warned = false;
    memset(floaters, 0, sizeof(floaters));
    go(MINE, 0);
    last_mine_save_ms = clock_ms;
    save_now();
}

/* ---- per-page actions ----------------------------------------------------- */
static void cycle(uint8_t *value, const uint8_t *steps, unsigned count) {
    unsigned i = 0;
    while (i < count && steps[i] != *value) ++i;
    *value = steps[(i + 1) % count];
}
static void activate(void) {
    char text[64];
    switch (page) {
        case HOME:
            if (selected == STATION_MINE) {
                if (game.run.active)
                    go(MINE, 0);
                else
                    start_dive(0);
            } else if (selected == STATION_SHOP)
                go(SHOP, 0);
            else if (selected == STATION_ELEVATOR)
                go(ELEVATOR, 0);
            else
                go(SETTINGS, 0);
            break;
        case MINE: react(gd_move(&game, GD_DOWN)); break;
        case PAUSE:
            if (selected == 0)
                go(MINE, 0);
            else if (selected == 1) {
                if (!game.profile.level[GD_DYNAMITE]) {
                    game_audio_effect(GD_SOUND_DENY);
                    show_notice("No dynamite: buy it at the shop");
                    return;
                }
                go(MINE, 0);
                react(gd_blast(&game));
            } else {
                gd_end_dive(&game, GD_END_ROPE);
                game_audio_effect(GD_SOUND_COINS);
                finish_dive();
            }
            break;
        case REPORT: go(HOME, STATION_MINE); break;
        case SHOP:
            if (selected >= GD_UPGRADE_COUNT) {
                go(HOME, STATION_SHOP);
                break;
            }
            {
                GdUpgrade u = (GdUpgrade)selected;
                uint64_t price = gd_upgrade_price(&game.profile, u);
                if (gd_buy_upgrade(&game, u)) {
                    game_audio_effect(GD_SOUND_COINS);
                    snprintf(text, sizeof(text), "%s %s", gd_upgrade_name(u),
                             u == GD_DYNAMITE ? "bought" : "upgraded");
                    show_notice(text);
                    save_now();
                } else {
                    game_audio_effect(GD_SOUND_DENY);
                    if (!price)
                        show_notice(u == GD_DYNAMITE ? "Dynamite box is full" : "Already at the best level");
                    else if (game.run.active && u != GD_DYNAMITE)
                        show_notice("Finish the dive first");
                    else {
                        snprintf(text, sizeof(text), "Need %llu more gold",
                                 (unsigned long long)(price - game.profile.gold));
                        show_notice(text);
                    }
                }
            }
            break;
        case ELEVATOR:
            if (selected >= GD_STOP_COUNT) {
                go(HOME, STATION_ELEVATOR);
            } else if (game.run.active) {
                game_audio_effect(GD_SOUND_DENY);
                show_notice("Finish the dive first");
            } else if (gd_stop_owned(&game.profile, selected)) {
                start_dive(selected);
            } else if (gd_buy_stop(&game, selected)) {
                game_audio_effect(GD_SOUND_COINS);
                show_notice("Elevator stop opened");
                save_now();
            } else {
                game_audio_effect(GD_SOUND_DENY);
                if (!gd_stop_reached(&game.profile, selected))
                    snprintf(text, sizeof(text), "Dig to %u m first", selected * GD_STOP_SPACING);
                else
                    snprintf(text, sizeof(text), "Need %llu gold",
                             (unsigned long long)gd_stop_price(selected));
                show_notice(text);
            }
            break;
        case SETTINGS: {
            static const uint8_t levels[] = {0, 20, 40, 60, 80, 100}, light[] = {10, 25, 50, 75, 100},
                                 sleeps[] = {0, 1, 2, 3, 4};
            if (selected == 0) cycle(&preferences[PREF_MUSIC], levels, sizeof(levels));
            if (selected == 1) cycle(&preferences[PREF_EFFECTS], levels, sizeof(levels));
            if (selected == 2) {
                cycle(&preferences[PREF_BRIGHTNESS], light, sizeof(light));
                bsp_display_backlight(preferences[PREF_BRIGHTNESS]);
            }
            if (selected == 3) cycle(&preferences[PREF_SLEEP], sleeps, sizeof(sleeps));
            if (selected <= 3) {
                game_audio_levels(preferences[PREF_MUSIC], preferences[PREF_EFFECTS]);
                save_preferences();
            } else if (selected == 4)
                go(RESET_CONFIRM, 0);
            else
                go(HOME, STATION_SETTINGS);
            break;
        }
        case RESET_CONFIRM:
            if (selected == 1) {
                gd_new_profile(&game);
                save_now();
                show_notice("Progress erased");
                go(HOME, STATION_MINE);
            } else
                go(SETTINGS, 4);
            break;
        default: break;
    }
}
static void back(void) {
    switch (page) {
        case MINE: go(PAUSE, 0); break;
        case PAUSE: go(MINE, 0); break;
        case REPORT: go(HOME, STATION_MINE); break;
        case SHOP: go(HOME, STATION_SHOP); break;
        case ELEVATOR: go(HOME, STATION_ELEVATOR); break;
        case SETTINGS: go(HOME, STATION_SETTINGS); break;
        case RESET_CONFIRM: go(SETTINGS, 4); break;
        default: break;
    }
}
void app_input(Input in) {
    last_input_ms = clock_ms;
    redraw_wanted = true;
    if (page == MINE && in != IN_BACK) {
        react(gd_move(&game, in == IN_UP ? GD_LEFT : in == IN_DOWN ? GD_RIGHT : GD_DOWN));
        return;
    }
    if (in == IN_BACK) {
        game_audio_effect(GD_SOUND_MENU);
        back();
        return;
    }
    unsigned rows = page_rows(page);
    if (in == IN_OK) {
        game_audio_effect(GD_SOUND_MENU);
        activate();
    } else if (rows > 1) {
        game_audio_effect(GD_SOUND_MENU);
        selected = in == IN_UP ? (selected + rows - 1) % rows : (selected + 1) % rows;
    }
}
void app_activate(int id) {
    last_input_ms = clock_ms;
    redraw_wanted = true;
    if (id == HIT_BACK || id == HIT_MENU) {
        app_input(IN_BACK);
    } else if (id == HIT_LEFT || id == HIT_RIGHT || id == HIT_DIG) {
        app_input(id == HIT_LEFT ? IN_UP : id == HIT_RIGHT ? IN_DOWN : IN_OK);
    } else if (id >= HIT_ROW && (unsigned)id < page_rows(page)) {
        selected = (unsigned)id;
        app_input(IN_OK);
    }
}

/* ---- pointer (desktop, Android) ------------------------------------------- */
static struct {
    bool down;
    int x, y, id, dragged;
} touch;
void pointer_event(int x, int y, PointerPhase phase) {
    if (phase == POINTER_DOWN) {
        touch.down = true;
        touch.x = x;
        touch.y = y;
        touch.dragged = 0;
        touch.id = hit_find(x, y);
    } else if (phase == POINTER_MOVE && touch.down) {
        int dy = y - touch.y;
        if (page != MINE && (dy > 24 || dy < -24)) { /* drag scrolls lists */
            pointer_scroll(dy > 0 ? -1 : 1);
            touch.y = y;
            touch.dragged = 1;
        }
    } else if (phase == POINTER_UP && touch.down) {
        touch.down = false;
        if (!touch.dragged && touch.id != -1 && hit_find(x, y) == touch.id) app_activate(touch.id);
    } else
        touch.down = false;
}
void pointer_scroll(int steps) {
    if (page == MINE || page == REPORT) return;
    while (steps > 0) {
        app_input(IN_DOWN);
        --steps;
    }
    while (steps < 0) {
        app_input(IN_UP);
        ++steps;
    }
}
bool pointer_back(void) {
    if (page == HOME) return false;
    app_input(IN_BACK);
    return true;
}

/* ---- buttons -------------------------------------------------------------- */
typedef struct {
    uint8_t button, event;
} ButtonEvent;
static void input_callback(bsp_btn_t button, bsp_btn_ev_t event, void *user) {
    (void)user; /* runs in the button driver: queue only, never block */
    ButtonEvent e = {(uint8_t)button, (uint8_t)event};
    xQueueSend(inputs, &e, 0);
}
static void button_event(ButtonEvent e) {
    if (wake_guard) return;
    if (e.event == BSP_BTN_RELEASE) {
        if ((int)e.button == repeat_button) repeat_button = -1;
        return;
    }
    if (e.event != BSP_BTN_CLICK && e.event != BSP_BTN_LONG) return;
    if (screen_dark || screen_dimmed) { /* the first press only wakes the screen */
        screen_dark = screen_dimmed = false;
        bsp_display_backlight(preferences[PREF_BRIGHTNESS]);
        last_input_ms = clock_ms;
        redraw_wanted = true;
        return;
    }
    if (e.event == BSP_BTN_LONG) {
        if (e.button == BSP_BTN_OK)
            app_input(IN_BACK);
        else { /* hold UP/DOWN: repeat */
            repeat_button = e.button;
            repeat_next_ms = clock_ms;
        }
        return;
    }
    app_input(e.button == BSP_BTN_UP ? IN_UP : e.button == BSP_BTN_DOWN ? IN_DOWN : IN_OK);
}

/* ---- Passport power: dim, screen off, deep sleep -------------------------- */
static bool sleep_error(const char *step, esp_err_t err) {
    if (err == ESP_OK) return false;
    ESP_LOGE(TAG, "Sleep preparation %s: %s; restarting from the saved game", step, esp_err_to_name(err));
    esp_restart();
    return true;
}
static void enter_sleep(void) {
    if (bsp_button_read_mv() < BSP_BTN_MV_RELEASED_MIN) return; /* a key is held */
    if (!save_now()) {
        last_input_ms = clock_ms; /* try again later rather than lose progress */
        return;
    }
    if (esp_deep_sleep_enable_gpio_wakeup(1ULL << BSP_BTN_GPIO, ESP_GPIO_WAKEUP_GPIO_LOW) != ESP_OK) {
        last_input_ms = clock_ms;
        return;
    }
    if (!bsp_lvgl_lock(1000)) return;
    int level = 0;
    if (sleep_error("buttons", bsp_button_prepare_deep_sleep(&level))) return;
    if (level != 1) {
        esp_restart();
        return;
    }
    if (sleep_error("battery", bsp_battery_sleep())) return;
    if (sleep_error("audio task", game_audio_stop())) return;
    if (sleep_error("audio", bsp_audio_sleep())) return;
    if (sleep_error("I2S", bsp_audio_prepare_deep_sleep())) return;
    if (sleep_error("I2C", bsp_i2c_prepare_deep_sleep())) return;
    if (sleep_error("display", bsp_display_prepare_deep_sleep())) return;
    ESP_LOGI(TAG, "Saved; deep sleep until a button is pressed");
    esp_deep_sleep_start();
    esp_restart(); /* never returns to the UI */
}
static void power_tick(void) {
    if (pointer_mode) return; /* hosts never dim, sleep or read a battery */
    static uint32_t last_battery = 0, last_usb = 0;
    if (wake_guard) { /* wait for the waking press to be released */
        if (bsp_button_read_mv() < BSP_BTN_MV_RELEASED_MIN)
            wake_released_ms = clock_ms;
        else if (clock_ms - wake_released_ms >= 250) {
            wake_guard = false;
            last_input_ms = clock_ms;
        }
    }
    if (clock_ms - last_usb >= 500) { /* USB start-of-frame counter moves only on a host connection */
        last_usb = clock_ms;
        uint32_t frame = REG_READ(USB_SERIAL_JTAG_FRAM_NUM_REG) & USB_SERIAL_JTAG_SOF_FRAME_INDEX;
        if (frame != usb_frame) usb_seen_ms = clock_ms;
        usb_frame = frame;
        bool powered = usb_seen_ms && clock_ms - usb_seen_ms < 1500;
        if (powered != battery_charging) {
            battery_charging = powered;
            redraw_wanted = true;
        }
    }
    if (!last_battery || clock_ms - last_battery >= 30000) {
        last_battery = clock_ms ? clock_ms : 1;
        int soc = bsp_battery_soc();
        if (soc != battery_percent) redraw_wanted = true;
        battery_percent = soc;
    }
    unsigned off = sleep_seconds[preferences[PREF_SLEEP]], idle = clock_ms - last_input_ms;
    if (!off) return;
    unsigned dim = off / 2 < 15 ? 15 : off / 2;
    if (idle >= off * 1000u) {
        if (!battery_charging && !wake_guard) {
            enter_sleep();
            return;
        }
        if (!screen_dark) {
            screen_dark = true;
            bsp_display_backlight(0);
        }
    } else if (!screen_dimmed && !screen_dark && idle >= dim * 1000u) {
        screen_dimmed = true;
        bsp_display_backlight(preferences[PREF_BRIGHTNESS] < 15 ? preferences[PREF_BRIGHTNESS] : 15);
    }
}

/* ---- startup and one main-loop iteration ---------------------------------- */
bool app_start(void) {
    gd_init(&game);
    gd_save_set_version(firmware_version());
    esp_err_t err = storage_init();
    if (err == ESP_OK) {
        err = storage_load(&game);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            save_enabled = false; /* never overwrite a save we could not read */
            gd_init(&game);
            show_notice(err == ESP_ERR_INVALID_VERSION ? "Save is newer: please update" : "Save unreadable");
            notice_until = 8000;
        }
    } else {
        save_enabled = false;
        show_notice("Storage unavailable");
    }
    ESP_LOGI(TAG, "Gold Digger %s: storage=%s gold=%llu", firmware_version(), esp_err_to_name(err),
             (unsigned long long)game.profile.gold);
    wake_guard = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
    if (game.run.active) go(MINE, 0);
    inputs = xQueueCreate(16, sizeof(ButtonEvent));
    if (!inputs) return false;
    ESP_ERROR_CHECK(bsp_display_init());
    if (!bsp_lvgl_init()) {
        ESP_LOGE(TAG, "LVGL initialization failed");
        return false;
    }
    if (bsp_battery_init() != ESP_OK) battery_percent = -1;
    storage_preferences_load(preferences);
    if (!bsp_lvgl_lock(2000)) return false;
    build_ui();
    render();
    bsp_lvgl_unlock();
    game_audio_start(preferences[PREF_MUSIC], preferences[PREF_EFFECTS]);
    bsp_display_backlight(preferences[PREF_BRIGHTNESS]);
    ESP_ERROR_CHECK(bsp_button_init(input_callback, NULL));
    loop_previous = esp_timer_get_time();
    return true;
}
void app_frame(void) {
    int64_t now = esp_timer_get_time();
    clock_ms += (uint32_t)((now - loop_previous) / 1000);
    loop_previous = now;
    ButtonEvent e;
    while (xQueueReceive(inputs, &e, 0) == pdTRUE) button_event(e);
    if (repeat_button >= 0 && clock_ms >= repeat_next_ms && !wake_guard) {
        repeat_next_ms = clock_ms + (page == MINE ? 170 : 120);
        app_input(repeat_button == BSP_BTN_UP ? IN_UP : IN_DOWN);
    }
    if (page == MINE && clock_ms - last_mine_save_ms > 20000) {
        last_mine_save_ms = clock_ms;
        save_now();
    }
    game_audio_music(page == MINE || page == PAUSE ? GD_MUSIC_MINE : GD_MUSIC_SURFACE);
    power_tick();
    /* Animated screens (home sky, lava, floating text) redraw at 10 Hz. */
    static uint32_t next_anim;
    if (clock_ms >= next_anim) {
        next_anim = clock_ms + 100;
        redraw_wanted = true;
    }
    if (redraw_wanted && !screen_dark && bsp_lvgl_lock(100)) {
        redraw_wanted = false;
        render();
        bsp_lvgl_unlock();
    }
}
void app_main(void) {
    if (!app_start()) return;
    for (;;) {
        app_frame();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
