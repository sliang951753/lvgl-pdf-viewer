/**
 * lv_conf.h
 * LVGL 9.5 configuration for lvgl-pdf-viewer
 *
 * Cross-platform: Windows (MSVC, Win32) + Linux (SDL2, T507 aarch64)
 * Color depth: 32-bit ARGB8888  — matches MuPDF BGRA output directly.
 */

/* clang-format off */
#if 1  /* Set to 0 to disable this file (use lv_conf_template.h as reference) */

#ifndef LV_CONF_H
#define LV_CONF_H

#if !defined(__ASSEMBLY__) && !defined(__ASSEMBLER__)
#include <stdint.h>
#endif

/*====================
 * Color settings
 *====================*/

/** Color depth: 8, 16, 24, or 32 */
#define LV_COLOR_DEPTH 32

/*====================
 * Memory settings
 *====================*/

/** 1: Use custom malloc/free (set LV_MEM_CUSTOM_* below).
 *  0: Use LVGL's internal pool allocator. */
#define LV_MEM_CUSTOM 0

/** Size of LVGL's internal heap (bytes). 4 MB is comfortable for PDF UI. */
#define LV_MEM_SIZE (4 * 1024 * 1024U)

/*====================
 * HAL settings
 *====================*/

/** Default display refresh period (ms) */
#define LV_DEF_REFR_PERIOD 16   /* ~60 fps */

/** Dot-per-inch of the display. Used for font scaling hints. */
#define LV_DPI_DEF 100

/*====================
 * Feature enable/disable
 *====================*/

/* Drawing */
#define LV_USE_DRAW_SW 1          /* Software renderer (required) */
#define LV_USE_DRAW_SW_COMPLEX 1  /* Enable complex SW draw ops   */

/* Widgets we actually use */
#define LV_USE_LABEL     1
#define LV_USE_IMAGE     1
#define LV_USE_BUTTON    1
#define LV_USE_SCROLL    1

/* Layouts */
#define LV_USE_FLEX      1
#define LV_USE_GRID      1

/* Animation (for scroll) */
#define LV_USE_ANIMATION 1

/* Logging */
#define LV_USE_LOG       1
#define LV_LOG_LEVEL     LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF    1

/* Profiler / perf monitor (enable during development) */
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

/*====================
 * Platform drivers
 *====================*/

#ifdef _WIN32
/* LVGL built-in Win32 window driver */
#define LV_USE_WINDOWS   1
#define LV_USE_SDL       0
#else
/* SDL2 driver */
#define LV_USE_SDL       1
#define LV_USE_WINDOWS   0
#endif

/*====================
 * Font settings
 *====================*/

/** Built-in Montserrat font (used for UI labels) */
#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  1
#define LV_FONT_MONTSERRAT_20  1

/** Default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
 * Miscellaneous
 *====================*/

/** Enable GPU-independent screenshot for testing */
#define LV_USE_SNAPSHOT 0

/** Assert on error */
#define LV_USE_ASSERT_NULL    1
#define LV_USE_ASSERT_MALLOC  1
#define LV_USE_ASSERT_MEM_INTEGRITY 0

#endif /* LV_CONF_H */
#endif /* End of "Content enable" */
