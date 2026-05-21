/* =============================================================================
 * platform_sdl2.c  –  LVGL 9.5 SDL2 driver initialisation
 *
 * Used on Linux / Allwinner T507 (and any desktop Linux dev machine).
 * Requires:  SDL2 (libsdl2-dev or Timesys SDK equivalent)
 *            LVGL built with LV_USE_SDL=1 (set in lv_conf.h)
 * =========================================================================== */

#ifndef _WIN32

#include "platform.h"
#include "lv_conf.h"
#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"   /* LVGL 9 SDL2 driver header */
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_mousewheel.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static lv_display_t *g_disp         = NULL;
static lv_indev_t   *g_mouse_indev  = NULL;
static lv_indev_t   *g_keybd_indev  = NULL;
static lv_indev_t   *g_wheel_indev  = NULL;

static uint32_t g_last_tick = 0;

void platform_init(int width, int height)
{
    /* SDL2 init (video + events; audio not needed) */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "[platform_sdl2] SDL_Init error: %s\n", SDL_GetError());
        exit(1);
    }

    /* LVGL 9 display driver for SDL2 */
    g_disp = lv_sdl_window_create(width, height);
    if (g_disp == NULL) {
        fprintf(stderr, "[platform_sdl2] lv_sdl_window_create(%d,%d) failed\n",
                width, height);
        exit(1);
    }

    /* Set window title */
    lv_sdl_window_set_title(g_disp, "LVGL PDF Viewer");

    /* Input devices */
    g_mouse_indev = lv_sdl_mouse_create();
    g_wheel_indev = lv_sdl_mousewheel_create();
    g_keybd_indev = lv_sdl_keyboard_create();

    g_last_tick = SDL_GetTicks();

    printf("[platform_sdl2] Window %dx%d created OK\n", width, height);
}

void platform_loop_once(void)
{
    /* Advance LVGL tick by elapsed ms */
    uint32_t now  = SDL_GetTicks();
    uint32_t diff = now - g_last_tick;
    if (diff > 0) {
        lv_tick_inc(diff);
        g_last_tick = now;
    }

    /* Run LVGL timer tasks */
    uint32_t sleep_ms = lv_timer_handler();
    if (sleep_ms == 0) sleep_ms = 1;
    SDL_Delay(sleep_ms < 10 ? sleep_ms : 10);
}

bool platform_should_quit(void)
{
    /* IMPORTANT:
     * Do NOT drain SDL's global event queue here with SDL_PollEvent().
     * LVGL's SDL input driver (mouse/touch/keyboard) also consumes events
     * from the same queue. If we drain it first, touch/click appears broken.
     *
     * Only peek SDL_QUIT without removing events, so LVGL still receives input.
     */
    SDL_Event e;
    int n = SDL_PeepEvents(&e, 1, SDL_PEEKEVENT, SDL_QUIT, SDL_QUIT);
    return n > 0;
}

lv_display_t *platform_get_display(void)
{
    return g_disp;
}

lv_indev_t *platform_get_touch_indev(void)
{
    return g_mouse_indev;
}

#endif /* !_WIN32 */
