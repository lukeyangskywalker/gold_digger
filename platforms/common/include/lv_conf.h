/* LVGL configuration for desktop and Android: mirrors sdkconfig.defaults
   (16-bit colour, 48 KiB pool, the same Montserrat sizes) so layouts and memory
   behaviour match the Passport. */
#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH        16
#define LV_USE_OS             LV_OS_NONE
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_BUILTIN
#define LV_MEM_SIZE           (48 * 1024U)
#define LV_DEF_REFR_PERIOD    20
#define LV_FONT_MONTSERRAT_8  1
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_UNSCII_16     1
#define LV_USE_LOG            0
#define LV_USE_ASSERT_MALLOC  1
#define LV_USE_ASSERT_NULL    1
#define LV_USE_SYSMON         0
#define LV_USE_PERF_MONITOR   0
#define LV_USE_MEM_MONITOR    0
#endif
