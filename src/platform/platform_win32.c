/* =============================================================================
 * platform_win32.c  –  LVGL 9.5 Win32 driver initialisation
 *
 * Uses the lv_windows driver bundled with LVGL (lv_port_windows).
 * Compile only on WIN32 (guarded by CMake target_sources + preprocessor).
 * =========================================================================== */

#ifdef _WIN32

#include "platform.h"
#include "lv_conf.h"
#include "lvgl.h"
#include "lv_port_windows.h"  /* from third_party/lvgl/src/drivers/windows */

#include <windows.h>
#include <stdio.h>

static lv_display_t *g_disp = NULL;
static lv_indev_t   *g_mouse_indev  = NULL;
static lv_indev_t   *g_keybd_indev  = NULL;
static lv_indev_t   *g_encoder_indev = NULL;
static lv_indev_t   *g_touch_indev  = NULL;

void platform_init(int width, int height)
{
    /* lv_windows_window_create creates the Win32 window AND registers the
     * display / input devices with LVGL 9.x APIs internally.
     * zoom_level = 100 means 1:1 pixel mapping; allow_dpi_scale = true lets
     * Windows scale the window on high-DPI displays.                        */
    HWND hwnd = lv_windows_window_create(
        L"LVGL PDF Viewer",
        width,
        height,
        100,     /* zoom % */
        true     /* allow DPI scale */
    );

    if (hwnd == NULL) {
        fprintf(stderr, "[platform_win32] Failed to create Win32 window\n");
        exit(1);
    }

    /* Retrieve the LVGL display bound to that window */
    g_disp = lv_windows_get_display(hwnd);

    /* Input devices registered by the driver */
    g_mouse_indev   = lv_windows_get_mouse_indev(hwnd);
    g_keybd_indev   = lv_windows_get_keyboard_indev(hwnd);
    g_encoder_indev = lv_windows_get_encoder_indev(hwnd);

    if (g_disp == NULL) {
        fprintf(stderr, "[platform_win32] lv_windows_get_display() returned NULL\n");
        exit(1);
    }

    printf("[platform_win32] Window %dx%d created OK\n", width, height);
}

void platform_loop_once(void)
{
    /* Let LVGL process one tick (call before lv_timer_handler) */
    lv_tick_inc(1);   /* 1 ms tick – real timing comes from the thread below  */

    /* Pump Win32 messages; lv_windows drives its own message loop internally.
     * We only need lv_timer_handler() here.                                  */
    lv_timer_handler();

    /* Sleep ~1 ms so we don't spin-burn the CPU */
    Sleep(1);
}

bool platform_should_quit(void)
{
    /* lv_windows sets a flag when the window is closed */
    return !lv_windows_is_quit_msg_received() ? false : true;
}

lv_display_t *platform_get_display(void)
{
    return g_disp;
}

lv_indev_t *platform_get_touch_indev(void)
{
    /* Win32 demo: reuse mouse as touch */
    return g_mouse_indev;
}

#endif /* _WIN32 */
