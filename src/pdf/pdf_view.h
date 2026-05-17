/* =============================================================================
 * pdf_view.h  –  MuPDF rendering abstraction for LVGL
 *
 * Renders PDF pages into lv_image_dsc_t buffers and manages an LRU cache.
 * =========================================================================== */

#pragma once

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle */
typedef struct pdf_view_t pdf_view_t;

/**
 * @brief  Initialise the PDF subsystem (call once at startup).
 *         Allocates the MuPDF context with the given memory limit.
 * @param  mem_limit_mb  MuPDF store cache limit in MB (0 = default 256 MB)
 */
void pdf_view_sys_init(uint32_t mem_limit_mb);

/**
 * @brief  Shutdown the PDF subsystem (call at exit).
 */
void pdf_view_sys_deinit(void);

/**
 * @brief  Open a PDF file.
 * @param  path     Absolute or relative path to the PDF.
 * @return Opaque handle, or NULL on failure.
 */
pdf_view_t *pdf_view_open(const char *path);

/**
 * @brief  Close and free a PDF view handle.
 */
void pdf_view_close(pdf_view_t *view);

/**
 * @brief  Get total number of pages.
 */
int pdf_view_page_count(pdf_view_t *view);

/**
 * @brief  Render a page to an LVGL image descriptor.
 *
 * The returned lv_image_dsc_t is valid until the next call that evicts this
 * page from the LRU cache, or until pdf_view_close().  Do NOT free the buffer
 * yourself.
 *
 * @param  view      Open PDF view handle.
 * @param  page_idx  Zero-based page index.
 * @param  disp_w    Target display width  (used to calculate render scale).
 * @param  disp_h    Target display height.
 * @param  zoom      Zoom factor (1.0 = fit-to-width, 2.0 = 2x, etc.).
 * @return Pointer to a cached lv_image_dsc_t, or NULL on failure.
 */
const lv_image_dsc_t *pdf_view_render_page(pdf_view_t *view,
                                            int         page_idx,
                                            int         disp_w,
                                            int         disp_h,
                                            float       zoom);

/**
 * @brief  Prefetch adjacent pages into the cache (non-blocking hint).
 *         Call after rendering the current page.
 */
void pdf_view_prefetch(pdf_view_t *view, int page_idx, int disp_w, int disp_h, float zoom);

/**
 * @brief  Invalidate all cached pages (e.g. after a zoom change).
 */
void pdf_view_cache_clear(pdf_view_t *view);

/**
 * @brief  Return the last error string (thread-local, valid until next call).
 */
const char *pdf_view_last_error(void);

#ifdef __cplusplus
}
#endif
