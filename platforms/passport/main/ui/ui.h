/* Shared declarations for the Passport UI modules (app.c: state, input, power
   and the main loop; draw.c: every screen, drawn with LVGL draw primitives at
   240x320). Host ports compile these same modules against the shims in
   platforms/common/include and drive app_start()/app_frame(). */
#pragma once
#include "digger.h"
#include "storage.h"
#include "bsp_display.h"
#include "bsp_audio.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "soc/soc.h"
#include "soc/usb_serial_jtag_reg.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "../game_audio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef enum { HOME, MINE, PAUSE, REPORT, SHOP, ELEVATOR, SETTINGS, RESET_CONFIRM, PAGE_COUNT } Page;
typedef enum { STATION_SHOP, STATION_MINE, STATION_ELEVATOR, STATION_SETTINGS, STATION_COUNT } Station;
enum { SETTINGS_ROWS = 6, SHOP_ROWS = GD_UPGRADE_COUNT + 1, PAUSE_ROWS = 3 };

/* ---- state shared by the modules (app.c) ---------------------------------- */
extern GdGame game;
extern Page page;
extern unsigned selected;      /* focused row or station on the current page */
extern uint8_t preferences[PREF_COUNT];
extern uint32_t clock_ms;      /* UI clock for animation */
extern bool save_enabled, pointer_mode, battery_charging;
extern int battery_percent;    /* -1 unknown */
extern const char *TAG;
extern char notice[64];
extern uint32_t notice_until;
extern const unsigned sleep_seconds[5];

typedef struct {
    char text[20];
    uint32_t color, start;
    uint8_t x;
    uint16_t row;
} Floater;
enum { FLOATERS = 5 };
extern Floater floaters[FLOATERS];
extern uint32_t shake_until;

const char *firmware_version(void);
bool save_now(void);
void show_notice(const char *text);
unsigned page_rows(Page p);

/* ---- drawing (draw.c) ----------------------------------------------------- */
void build_ui(void);
void render(void);
/* Pointer hit regions are recorded while drawing; ids are page specific. */
enum { HIT_BACK = 1000, HIT_MENU, HIT_LEFT, HIT_RIGHT, HIT_DIG, HIT_ROW = 0 };
void hit_add(int x, int y, int w, int h, int id);
int hit_find(int x, int y);
void hit_clear(void);

/* ---- input (app.c) -------------------------------------------------------- */
typedef enum { IN_UP, IN_DOWN, IN_OK, IN_BACK } Input;
void app_input(Input in);
void app_activate(int id); /* pointer tap on a hit region */

/* ---- host entry points (pointer UI is compiled everywhere, used by hosts) - */
typedef enum { POINTER_DOWN, POINTER_MOVE, POINTER_UP, POINTER_CANCEL } PointerPhase;
void pointer_event(int x, int y, PointerPhase phase);
void pointer_scroll(int steps);
bool pointer_back(void); /* false on the home screen */
bool app_start(void);
void app_frame(void);
