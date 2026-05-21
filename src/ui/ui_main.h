/* =============================================================================
 * ui_main.h  –  LVGL PDF viewer top-level UI
 * =========================================================================== */

#pragma once

#include "lvgl.h"
#include "../pdf/pdf_view.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Create and populate the main PDF viewer screen.
 * @param  view    Open pdf_view_t handle.
 * @param  disp_w  Display width  in pixels.
 * @param  disp_h  Display height in pixels.
 */
void ui_main_create(pdf_view_t *view, int disp_w, int disp_h);

/**
 * @brief  Navigate to an absolute page index.
 */
void ui_main_goto_page(int page_idx);

/**
 * @brief  Navigate to a user-facing page number (1-based).
 * @param  page_no_1based  Desired page number in [1..page_count].
 * @return true on success, false if out of range or no document.
 */
bool ui_main_goto_page_number(int page_no_1based);

/**
 * @brief  Get current user-facing page number (1-based).
 */
int ui_main_current_page_number(void);

/**
 * @brief  Get total pages in current document.
 */
int ui_main_total_pages(void);

/**
 * @brief  Adjust zoom level (delta applied to current zoom, clamped to sane range).
 */
void ui_main_zoom(float delta);

#ifdef __cplusplus
}
#endif
