/* Host replacement for the ESP-IDF, FreeRTOS and Passport BSP APIs used by
   the Passport UI in platforms/passport/main/ui. Desktop, web and Android compile the real
   Passport UI against these declarations; gd_host.c implements them on top
   of the callbacks a platform passes to gd_host_init(). Nothing here talks
   to hardware: there is no deep sleep, battery gauge or NVS on a host. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int esp_err_t;
#define ESP_OK                0
#define ESP_FAIL              (-1)
#define ESP_ERR_NO_MEM        0x101
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE  0x104
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT       0x107
#define ESP_ERR_INVALID_CRC   0x109
#define ESP_ERR_INVALID_VERSION 0x10A
#define ESP_ERR_NVS_NOT_FOUND 0x1102
const char *esp_err_to_name(esp_err_t err);
void gd_host_log(char level, const char *tag, const char *fmt, ...);
#define ESP_LOGE(tag, ...) gd_host_log('E', tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) gd_host_log('W', tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) gd_host_log('I', tag, __VA_ARGS__)
#define ESP_ERROR_CHECK(x) ((void)(x))

/* FreeRTOS subset: the app owns one input queue and is driven from one thread. */
typedef struct SpHostQueue *QueueHandle_t;
#define pdTRUE            1
#define pdFALSE           0
#define pdPASS            1
#define pdMS_TO_TICKS(ms) (ms)
QueueHandle_t xQueueCreate(unsigned length, unsigned item_size);
int xQueueSend(QueueHandle_t q, const void *item, int ticks);
int xQueueReceive(QueueHandle_t q, void *item, int ticks);
void vTaskDelay(int ticks);

int64_t esp_timer_get_time(void);
uint32_t esp_random(void);

typedef struct {
    const char *version;
} esp_app_desc_t;
const esp_app_desc_t *esp_app_get_description(void);

/* Power: hosts never sleep. The USB-frame register keeps changing, so the UI
   treats the host as externally powered (no battery sleep, charging icon). */
#define ESP_SLEEP_WAKEUP_UNDEFINED 0
#define ESP_SLEEP_WAKEUP_GPIO      7
#define ESP_GPIO_WAKEUP_GPIO_LOW   0
int esp_sleep_get_wakeup_cause(void);
esp_err_t esp_deep_sleep_enable_gpio_wakeup(uint64_t mask, int mode);
void esp_deep_sleep_start(void);
void esp_restart(void);
#define USB_SERIAL_JTAG_FRAM_NUM_REG    0
#define USB_SERIAL_JTAG_SOF_FRAME_INDEX 0x7ffu
uint32_t gd_host_usb_frame(void);
#define REG_READ(reg) ((void)(reg), gd_host_usb_frame())

/* Passport BSP subset. */
typedef enum { BSP_BTN_UP = 0, BSP_BTN_DOWN, BSP_BTN_OK } bsp_btn_t;
typedef enum { BSP_BTN_PRESS = 0, BSP_BTN_CLICK, BSP_BTN_DOUBLE, BSP_BTN_LONG, BSP_BTN_RELEASE } bsp_btn_ev_t;
typedef void (*bsp_btn_cb_t)(bsp_btn_t btn, bsp_btn_ev_t ev, void *user);
#define BSP_LCD_W               240
#define BSP_LCD_H               320
#define BSP_LVGL_SCREEN_RADIUS  30
#define BSP_BTN_GPIO            0
#define BSP_BTN_MV_RELEASED_MIN 1900
#define BSP_BTN_LONG_PRESS_MS   500
esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user);
esp_err_t bsp_button_prepare_deep_sleep(int *level);
int bsp_button_read_mv(void);
esp_err_t bsp_display_init(void);
void bsp_display_backlight(uint8_t percent);
esp_err_t bsp_display_prepare_deep_sleep(void);
struct _lv_display_t;
struct _lv_display_t *bsp_lvgl_init(void);
bool bsp_lvgl_lock(int timeout_ms);
void bsp_lvgl_unlock(void);
esp_err_t bsp_battery_init(void);
esp_err_t bsp_battery_sleep(void);
int bsp_battery_soc(void);
int bsp_battery_mv(void);
esp_err_t bsp_battery_recover(void);
esp_err_t bsp_audio_sleep(void);
esp_err_t bsp_audio_prepare_deep_sleep(void);
void bsp_audio_set_volume(uint8_t percent);
esp_err_t bsp_i2c_prepare_deep_sleep(void);
