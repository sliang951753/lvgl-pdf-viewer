/* =============================================================================
 * platform.h  –  Cross-platform abstraction for LVGL display + input init
 *
 * Implementations:
 *   platform_win32.c   (WIN32)
 *   platform_sdl2.c    (Linux / T507)
 * =========================================================================== */

#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Create the native window and initialise LVGL display + input devices.
 * @param  width   Logical width  in pixels
 * @param  height  Logical height in pixels
 */
void platform_init(int width, int height);

/**
 * @brief  Run one iteration of the platform event + LVGL timer loop.
 *         Call this inside your main while(!quit) loop.
 */
void platform_loop_once(void);

/**
 * @brief  Returns true when the user has requested to close the window.
 */
bool platform_should_quit(void);

/**
 * @brief  Returns the lv_display_t* created by platform_init().
 */
lv_display_t *platform_get_display(void);

/**
 * @brief  Returns the primary pointer/touch indev (mouse on desktop).
 */
lv_indev_t *platform_get_touch_indev(void);

#ifdef __cplusplus
}
#endif
