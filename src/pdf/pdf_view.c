/* =============================================================================
 * pdf_view.c  –  MuPDF → LVGL rendering implementation
 *
 * Page LRU cache:  keeps up to PDF_CACHE_PAGES rendered bitmaps in memory.
 * Color format:    ARGB8888 (matches LV_COLOR_FORMAT_ARGB8888, depth=32).
 * Thread safety:   NOT thread-safe; call only from the LVGL task/thread.
 * =========================================================================== */

#include "pdf_view.h"

/* MuPDF public API */
#include "mupdf/fitz.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* -------------------------------------------------------------------------- */
/* Configuration                                                               */
/* -------------------------------------------------------------------------- */

#define PDF_CACHE_PAGES   5      /* max rendered pages kept in RAM            */
#define PDF_DEFAULT_STORE (256 * 1024 * 1024)  /* 256 MB MuPDF store         */

/* -------------------------------------------------------------------------- */
/* Internal types                                                              */
/* -------------------------------------------------------------------------- */

typedef struct cache_entry_t {
    int              page_idx;
    float            zoom;
    int              render_w;
    int              render_h;
    uint8_t         *buf;        /* ARGB8888 pixel buffer (malloc'd)          */
    lv_image_dsc_t   img_dsc;   /* points into buf                            */
    uint32_t         lru_tick;  /* monotonic use counter for eviction         */
} cache_entry_t;

struct pdf_view_t {
    fz_context  *ctx;
    fz_document *doc;
    int          page_count;

    cache_entry_t cache[PDF_CACHE_PAGES];
    uint32_t      lru_clock;
};

/* Thread-local error string */
static char g_last_error[256] = {0};

/* Global MuPDF context (shared across all open documents for font caching) */
static fz_context *g_fz_ctx = NULL;
static int         g_ref_count = 0;

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                            */
/* -------------------------------------------------------------------------- */

static void set_error(const char *msg)
{
    strncpy(g_last_error, msg ? msg : "(null)", sizeof(g_last_error) - 1);
    g_last_error[sizeof(g_last_error) - 1] = '\0';
}

static cache_entry_t *cache_find(pdf_view_t *view, int page_idx, float zoom)
{
    for (int i = 0; i < PDF_CACHE_PAGES; i++) {
        cache_entry_t *e = &view->cache[i];
        if (e->page_idx == page_idx && e->zoom == zoom && e->buf != NULL)
            return e;
    }
    return NULL;
}

static cache_entry_t *cache_evict_lru(pdf_view_t *view)
{
    /* Find entry with lowest lru_tick; prefer empty slots */
    int best = 0;
    for (int i = 0; i < PDF_CACHE_PAGES; i++) {
        if (view->cache[i].buf == NULL) return &view->cache[i];
        if (view->cache[i].lru_tick < view->cache[best].lru_tick)
            best = i;
    }
    free(view->cache[best].buf);
    view->cache[best].buf = NULL;
    return &view->cache[best];
}

/* -------------------------------------------------------------------------- */
/* Public API – system init/deinit                                             */
/* -------------------------------------------------------------------------- */

void pdf_view_sys_init(uint32_t mem_limit_mb)
{
    if (g_fz_ctx != NULL) { g_ref_count++; return; }

    size_t store = mem_limit_mb ? (size_t)mem_limit_mb * 1024 * 1024
                                : PDF_DEFAULT_STORE;
    g_fz_ctx = fz_new_context(NULL, NULL, store);
    if (g_fz_ctx == NULL) {
        set_error("fz_new_context failed");
        return;
    }
    fz_register_document_handlers(g_fz_ctx);
    g_ref_count = 1;
    printf("[pdf_view] MuPDF context created (store=%zu MB)\n", store >> 20);
}

void pdf_view_sys_deinit(void)
{
    if (--g_ref_count <= 0 && g_fz_ctx) {
        fz_drop_context(g_fz_ctx);
        g_fz_ctx = NULL;
        g_ref_count = 0;
    }
}

/* -------------------------------------------------------------------------- */
/* Public API – open/close                                                     */
/* -------------------------------------------------------------------------- */

pdf_view_t *pdf_view_open(const char *path)
{
    if (!g_fz_ctx) { set_error("pdf_view_sys_init not called"); return NULL; }

    pdf_view_t *view = calloc(1, sizeof(pdf_view_t));
    if (!view) { set_error("calloc failed"); return NULL; }

    view->ctx = g_fz_ctx;

    fz_try(view->ctx) {
        view->doc = fz_open_document(view->ctx, path);
        view->page_count = fz_count_pages(view->ctx, view->doc);
    }
    fz_catch(view->ctx) {
        set_error(fz_caught_message(view->ctx));
        free(view);
        return NULL;
    }

    printf("[pdf_view] Opened '%s' (%d pages)\n", path, view->page_count);
    return view;
}

void pdf_view_close(pdf_view_t *view)
{
    if (!view) return;
    pdf_view_cache_clear(view);
    fz_drop_document(view->ctx, view->doc);
    free(view);
}

int pdf_view_page_count(pdf_view_t *view)
{
    return view ? view->page_count : 0;
}

bool pdf_view_page_size(pdf_view_t *view, int page_idx,
                        float *w_pts, float *h_pts)
{
    if (!view || page_idx < 0 || page_idx >= view->page_count)
        return false;
    fz_page *page = NULL;
    bool ok = false;
    fz_try(view->ctx) {
        page = fz_load_page(view->ctx, view->doc, page_idx);
        fz_rect bounds = fz_bound_page(view->ctx, page);
        if (w_pts) *w_pts = bounds.x1 - bounds.x0;
        if (h_pts) *h_pts = bounds.y1 - bounds.y0;
        ok = true;
    } fz_always(view->ctx) {
        if (page) fz_drop_page(view->ctx, page);
    } fz_catch(view->ctx) {
        ok = false;
    }
    return ok;
}

/* -------------------------------------------------------------------------- */
/* Public API – render                                                         */
/* -------------------------------------------------------------------------- */

const lv_image_dsc_t *pdf_view_render_page(pdf_view_t *view,
                                            int         page_idx,
                                            int         disp_w,
                                            int         disp_h,
                                            float       zoom)
{
    (void)disp_h;  /* reserved for future fit-to-height mode */

    if (!view || page_idx < 0 || page_idx >= view->page_count) {
        set_error("invalid page index");
        return NULL;
    }

    /* Cache hit? */
    cache_entry_t *entry = cache_find(view, page_idx, zoom);
    if (entry) {
        entry->lru_tick = ++view->lru_clock;
        return &entry->img_dsc;
    }

    /* Get page size at 72 dpi */
    fz_rect  page_rect;
    fz_page *page = NULL;

    fz_try(view->ctx) {
        page = fz_load_page(view->ctx, view->doc, page_idx);
        page_rect = fz_bound_page(view->ctx, page);
    }
    fz_catch(view->ctx) {
        set_error(fz_caught_message(view->ctx));
        return NULL;
    }

    /* Scale so page width fits disp_w, then apply zoom */
    float page_w_pt = page_rect.x1 - page_rect.x0;
    float scale     = ((float)disp_w / page_w_pt) * zoom;

    fz_matrix  mat    = fz_scale(scale, scale);
    fz_irect   bbox   = fz_round_rect(fz_transform_rect(page_rect, mat));
    int        rend_w = bbox.x1 - bbox.x0;
    int        rend_h = bbox.y1 - bbox.y0;

    /* Allocate pixel buffer */
    size_t buf_size = (size_t)rend_w * rend_h * 4;  /* BGRA8888 from MuPDF */
    uint8_t *buf = malloc(buf_size);
    if (!buf) {
        fz_drop_page(view->ctx, page);
        set_error("malloc failed for pixel buffer");
        return NULL;
    }

    /* Render via MuPDF */
    fz_pixmap *pix = NULL;

    fz_try(view->ctx) {
        /* Use BGR colorspace – MuPDF stores as BGRA; LVGL ARGB8888 is BGRA
         * in little-endian memory, so no byte-swap needed.                  */
        pix = fz_new_pixmap_with_bbox_and_data(
                view->ctx, fz_device_bgr(view->ctx), bbox, NULL, 1, buf);
        fz_clear_pixmap_with_value(view->ctx, pix, 0xFF);

        fz_device *dev = fz_new_draw_device(view->ctx, fz_identity, pix);
        fz_run_page(view->ctx, page, dev, mat, NULL);
        fz_close_device(view->ctx, dev);
        fz_drop_device(view->ctx, dev);
    }
    fz_catch(view->ctx) {
        set_error(fz_caught_message(view->ctx));
        fz_drop_pixmap(view->ctx, pix);
        fz_drop_page(view->ctx, page);
        free(buf);
        return NULL;
    }

    fz_drop_pixmap(view->ctx, pix);
    fz_drop_page(view->ctx, page);

    /* Store in cache */
    entry = cache_evict_lru(view);
    entry->page_idx = page_idx;
    entry->zoom     = zoom;
    entry->render_w = rend_w;
    entry->render_h = rend_h;
    entry->buf      = buf;
    entry->lru_tick = ++view->lru_clock;

    /* Build LVGL image descriptor */
    lv_image_dsc_t *dsc = &entry->img_dsc;
    memset(dsc, 0, sizeof(*dsc));
    dsc->header.magic   = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf      = LV_COLOR_FORMAT_ARGB8888;
    dsc->header.w       = (uint32_t)rend_w;
    dsc->header.h       = (uint32_t)rend_h;
    dsc->header.stride  = (uint32_t)(rend_w * 4);
    dsc->data_size      = (uint32_t)buf_size;
    dsc->data           = buf;

    printf("[pdf_view] Rendered page %d  %dx%d  zoom=%.2f\n",
           page_idx, rend_w, rend_h, zoom);
    return dsc;
}

void pdf_view_prefetch(pdf_view_t *view,
                       int page_idx, int disp_w, int disp_h, float zoom)
{
    /* Prefetch next page */
    if (page_idx + 1 < view->page_count)
        pdf_view_render_page(view, page_idx + 1, disp_w, disp_h, zoom);
    /* Prefetch previous page */
    if (page_idx - 1 >= 0)
        pdf_view_render_page(view, page_idx - 1, disp_w, disp_h, zoom);
}

void pdf_view_cache_clear(pdf_view_t *view)
{
    if (!view) return;
    for (int i = 0; i < PDF_CACHE_PAGES; i++) {
        if (view->cache[i].buf) {
            free(view->cache[i].buf);
            view->cache[i].buf = NULL;
        }
    }
}

const char *pdf_view_last_error(void)
{
    return g_last_error;
}

/* -------------------------------------------------------------------------- */
/* Text search                                                                 */
/* -------------------------------------------------------------------------- */

#define PDF_SEARCH_PAGE_HIT_MAX 64

static int append_hits(pdf_search_result_t *res,
                       int page,
                       const fz_quad *quads, int n,
                       int max_hits)
{
    for (int i = 0; i < n; i++) {
        if (max_hits > 0 && res->count >= max_hits) {
            res->truncated = 1;
            return 1; /* signal stop */
        }
        if (res->count >= res->capacity) {
            int new_cap = res->capacity ? res->capacity * 2 : 16;
            pdf_search_hit_t *grown =
                (pdf_search_hit_t *)realloc(res->hits,
                                            (size_t)new_cap * sizeof(*grown));
            if (!grown) {
                set_error("search: out of memory");
                res->truncated = 1;
                return 1;
            }
            res->hits = grown;
            res->capacity = new_cap;
        }
        const fz_quad q = quads[i];
        /* Convert quad → axis-aligned bounding rect (PDF point space). */
        float x0 = fminf(fminf(q.ul.x, q.ur.x), fminf(q.ll.x, q.lr.x));
        float y0 = fminf(fminf(q.ul.y, q.ur.y), fminf(q.ll.y, q.lr.y));
        float x1 = fmaxf(fmaxf(q.ul.x, q.ur.x), fmaxf(q.ll.x, q.lr.x));
        float y1 = fmaxf(fmaxf(q.ul.y, q.ur.y), fmaxf(q.ll.y, q.lr.y));
        pdf_search_hit_t *h = &res->hits[res->count++];
        h->page = page;
        h->x = x0;
        h->y = y0;
        h->w = x1 - x0;
        h->h = y1 - y0;
    }
    return 0;
}

pdf_search_result_t *pdf_view_search(pdf_view_t *view,
                                     const char *needle,
                                     int         start_page,
                                     bool        wrap_around,
                                     int         max_hits)
{
    if (!view || !needle || !*needle) {
        set_error("search: invalid arguments");
        return NULL;
    }
    if (start_page < 0) start_page = 0;
    if (start_page >= view->page_count) start_page = 0;

    pdf_search_result_t *res =
        (pdf_search_result_t *)calloc(1, sizeof(*res));
    if (!res) {
        set_error("search: alloc failed");
        return NULL;
    }

    fz_quad quads[PDF_SEARCH_PAGE_HIT_MAX];
    int total = view->page_count;
    int scanned = 0;
    int p = start_page;
    int stop = 0;

    while (scanned < total && !stop) {
        int n = 0;
        fz_try(view->ctx) {
            n = fz_search_page_number(view->ctx, view->doc, p,
                                      needle, NULL,
                                      quads, PDF_SEARCH_PAGE_HIT_MAX);
        } fz_catch(view->ctx) {
            n = 0; /* skip page on error */
        }
        if (n > 0) {
            if (append_hits(res, p, quads, n, max_hits))
                stop = 1;
        }
        scanned++;
        p++;
        if (p >= total) {
            if (!wrap_around) break;
            p = 0;
        }
    }

    return res;
}

void pdf_search_result_free(pdf_search_result_t *res)
{
    if (!res) return;
    free(res->hits);
    free(res);
}

pdf_search_result_t *pdf_view_search_page(pdf_view_t *view,
                                          int         page_idx,
                                          const char *needle,
                                          int         max_hits)
{
    if (!view || !needle || !*needle ||
        page_idx < 0 || page_idx >= view->page_count) {
        set_error("search_page: invalid arguments");
        return NULL;
    }
    pdf_search_result_t *res =
        (pdf_search_result_t *)calloc(1, sizeof(*res));
    if (!res) {
        set_error("search_page: alloc failed");
        return NULL;
    }
    fz_quad quads[PDF_SEARCH_PAGE_HIT_MAX];
    int n = 0;
    fz_try(view->ctx) {
        n = fz_search_page_number(view->ctx, view->doc, page_idx,
                                  needle, NULL,
                                  quads, PDF_SEARCH_PAGE_HIT_MAX);
    } fz_catch(view->ctx) {
        n = 0;
    }
    if (n > 0)
        append_hits(res, page_idx, quads, n, max_hits);
    return res;
}
