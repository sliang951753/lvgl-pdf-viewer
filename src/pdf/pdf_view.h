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
 * @brief  Get natural page size in PDF points (1 pt = 1/72 inch).
 *         Use this to convert search hit bboxes (point space) into pixel
 *         offsets for the rendered image:
 *
 *             scale_x = rendered_image_w / page_w_pts;
 *             scale_y = rendered_image_h / page_h_pts;
 *
 * @return true on success; false (and outputs untouched) on bad page.
 */
bool pdf_view_page_size(pdf_view_t *view, int page_idx,
                        float *w_pts, float *h_pts);

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

/* ------------------------------------------------------------------ */
/* Text search                                                         */
/* ------------------------------------------------------------------ */

/** A single match location in PDF page space (origin top-left, points). */
typedef struct {
    int   page;     /**< zero-based page index */
    float x, y;     /**< top-left of bounding rect, in PDF points */
    float w, h;     /**< width / height, in PDF points */
} pdf_search_hit_t;

typedef struct {
    pdf_search_hit_t *hits;
    int               count;     /**< number of valid hits */
    int               capacity;  /**< allocated capacity (>= count) */
    int               truncated; /**< 1 if max_hits limit was reached */
} pdf_search_result_t;

/**
 * @brief  Search the document for `needle` (case-insensitive substring,
 *         no regex). Search progresses page-by-page starting from
 *         `start_page` and wraps around when `wrap_around` is true.
 *
 * @param  view         Open PDF view handle.
 * @param  needle       UTF-8 query string. Empty/NULL → returns NULL.
 * @param  start_page   Zero-based page to start scanning.
 * @param  wrap_around  If true, wrap to page 0 after reaching the last page
 *                      and continue until start_page-1.
 * @param  max_hits     Hard cap on accumulated hits (stops scanning early).
 * @return Heap-allocated result; caller must free with pdf_search_result_free().
 *         NULL on allocation/argument error (use pdf_view_last_error()).
 */
pdf_search_result_t *pdf_view_search(pdf_view_t *view,
                                     const char *needle,
                                     int         start_page,
                                     bool        wrap_around,
                                     int         max_hits);

/** Free a search result returned by pdf_view_search(). NULL-safe. */
void pdf_search_result_free(pdf_search_result_t *res);

/**
 * @brief  Search a single page (used by UI for per-page highlight overlays
 *         without re-scanning the whole document).
 *
 * @return Heap-allocated result with hits only on this page (count may be 0).
 *         NULL on bad arguments. Caller frees with pdf_search_result_free().
 */
pdf_search_result_t *pdf_view_search_page(pdf_view_t *view,
                                          int         page_idx,
                                          const char *needle,
                                          int         max_hits);

#ifdef __cplusplus
}
#endif
