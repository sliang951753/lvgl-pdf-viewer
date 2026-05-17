/* =============================================================================
 * ui_main.h  –  LVGL PDF viewer top-level UI
 * =========================================================================== */

#pragma once

#include "lvgl.h"
#include "../pdf/pdf_view.h"

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
 * @brief  Adjust zoom level (delta applied to current zoom, clamped to sane range).
 */
void ui_main_zoom(float delta);

#ifdef __cplusplus
}
#endif
