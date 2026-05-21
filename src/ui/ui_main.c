/* =============================================================================
 * ui_main.c  –  LVGL 9.5 PDF viewer interface
 *
 * Layout (single screen):
 *
 *   ┌──────────────────────────────┐
 *   │  Top bar: filename + page N/N│  ← lv_label (LVGL flex row)
 *   ├──────────────────────────────┤
 *   │                              │
 *   │   PDF page image             │  ← lv_image inside lv_obj (scrollable)
 *   │   (vertically scrollable)    │
 *   │                              │
 *   ├──────────────────────────────┤
 *   │  [◀ Prev]    zoom  [Next ▶]  │  ← bottom nav bar
 *   └──────────────────────────────┘
 * =========================================================================== */

#include "ui_main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* State                                                                       */
/* -------------------------------------------------------------------------- */

static pdf_view_t  *g_view     = NULL;
static int          g_disp_w   = 800;
static int          g_disp_h   = 480;
static int          g_cur_page = 0;
static float        g_zoom     = 1.0f;
static bool         g_window_reflow_guard = false;

/* LVGL objects */
static lv_obj_t *g_screen      = NULL;
static lv_obj_t *g_topbar      = NULL;
static lv_obj_t *g_lbl_title   = NULL;
static lv_obj_t *g_lbl_page    = NULL;
static lv_obj_t *g_scroll_area = NULL;
static lv_obj_t *g_spacer      = NULL;      /* invisible, sized to full doc height */
static lv_obj_t *g_img         = NULL;      /* current/top page in window */
static lv_obj_t *g_img_next    = NULL;      /* next page in window */

/* Virtual full-document scroll: pixel Y offset of each page (0..N).
 * g_page_y[i] is top of page i; g_page_y[N] is total document height. */
static int32_t *g_page_y       = NULL;
static int32_t  g_doc_total_h  = 0;
static lv_obj_t *g_navbar      = NULL;
static lv_obj_t *g_btn_prev    = NULL;
static lv_obj_t *g_btn_next    = NULL;
static lv_obj_t *g_lbl_zoom    = NULL;
static lv_obj_t *g_btn_zoom_in = NULL;
static lv_obj_t *g_btn_zoom_out= NULL;

/* Page-jump dialog */
static lv_obj_t *g_jump_modal  = NULL;
static lv_obj_t *g_jump_input  = NULL;
static lv_obj_t *g_jump_status = NULL;
static lv_obj_t *g_jump_kb     = NULL;

/* Search */
static lv_obj_t *g_btn_search    = NULL;
static lv_obj_t *g_search_modal  = NULL;
static lv_obj_t *g_search_input  = NULL;
static lv_obj_t *g_search_status = NULL;
static lv_obj_t *g_search_kb     = NULL;

static pdf_search_result_t *g_search_hits = NULL;  /* doc-wide cached */
static int   g_search_cursor = -1;                 /* index into g_search_hits */
static char  g_search_query[128] = {0};


#define TOPBAR_H  48
#define NAVBAR_H  56
#define ZOOM_STEP 0.25f
#define ZOOM_MIN  0.5f
#define ZOOM_MAX  4.0f

/* -------------------------------------------------------------------------- */
/* Forward declarations                                                        */
/* -------------------------------------------------------------------------- */

static void render_current_page(void);
static void render_two_page_window(int top_page_idx);
static void rebuild_page_offsets(void);
static void position_window_images(void);
static void update_nav_labels(void);
static void cb_scroll(lv_event_t *e);
static int  page_height_for_zoom(int page_idx, float zoom);
static void show_page_jump_dialog(void);
static void close_page_jump_dialog(void);
static void cb_jump_confirm(lv_event_t *e);
static void cb_jump_cancel(lv_event_t *e);
static void cb_page_label_clicked(lv_event_t *e);

static void show_search_dialog(void);
static void close_search_dialog(void);
static void cb_search_run(lv_event_t *e);
static void cb_search_prev(lv_event_t *e);
static void cb_search_next(lv_event_t *e);
static void cb_search_close(lv_event_t *e);
static void cb_btn_search_clicked(lv_event_t *e);
static void search_clear_results(void);
static void search_jump_to_cursor(void);


/* -------------------------------------------------------------------------- */
/* Event callbacks                                                             */
/* -------------------------------------------------------------------------- */

static void cb_btn_prev(lv_event_t *e)
{
    (void)e;
    if (g_cur_page > 0 && g_page_y) {
        lv_obj_scroll_to_y(g_scroll_area, g_page_y[g_cur_page - 1], LV_ANIM_OFF);
    }
}

static void cb_btn_next(lv_event_t *e)
{
    (void)e;
    int page_count = pdf_view_page_count(g_view);
    if (g_cur_page < page_count - 1 && g_page_y) {
        lv_obj_scroll_to_y(g_scroll_area, g_page_y[g_cur_page + 1], LV_ANIM_OFF);
    }
}

static void cb_btn_zoom_in(lv_event_t *e)
{
    (void)e;
    ui_main_zoom(+ZOOM_STEP);
}

static void cb_btn_zoom_out(lv_event_t *e)
{
    (void)e;
    ui_main_zoom(-ZOOM_STEP);
}

static void cb_scroll(lv_event_t *e)
{
    if (g_window_reflow_guard) return;
    if (!g_page_y) return;

    lv_obj_t *obj = lv_event_get_target_obj(e);
    int page_count = pdf_view_page_count(g_view);
    if (page_count <= 0) return;

    int32_t y = lv_obj_get_scroll_y(obj);

    /* Find which page the top of the viewport is currently on
     * via binary search over g_page_y. */
    int lo = 0, hi = page_count - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (g_page_y[mid] <= y) lo = mid;
        else hi = mid - 1;
    }
    int new_top = lo;

    /* Reflow the 2-image window only when the top page changes.
     * Coordinates of images are absolute Y inside the scroll content,
     * so the scroll bar / scroll position remain correct automatically. */
    if (new_top != g_cur_page) {
        g_window_reflow_guard = true;
        g_cur_page = new_top;
        render_two_page_window(g_cur_page);
        g_window_reflow_guard = false;
    }
}

static void cb_scroll_end(lv_event_t *e)
{
    (void)e;
}

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                            */
/* -------------------------------------------------------------------------- */

static int page_height_for_zoom(int page_idx, float zoom)
{
    /* Predict pixel height from PDF point size + scale, no full render. */
    if (!g_view) return g_disp_h;
    float pw_pt = 0, ph_pt = 0;
    if (!pdf_view_page_size(g_view, page_idx, &pw_pt, &ph_pt) || pw_pt <= 0) {
        return g_disp_h;
    }
    float scale = ((float)g_disp_w / pw_pt) * zoom;
    int h = (int)(ph_pt * scale + 0.5f);
    return h > 0 ? h : g_disp_h;
}

static void rebuild_page_offsets(void)
{
    if (!g_view) return;
    int n = pdf_view_page_count(g_view);
    if (n <= 0) return;

    free(g_page_y);
    g_page_y = (int32_t *)malloc(sizeof(int32_t) * (n + 1));
    if (!g_page_y) { g_doc_total_h = 0; return; }

    int32_t y = 0;
    for (int i = 0; i < n; ++i) {
        g_page_y[i] = y;
        y += page_height_for_zoom(i, g_zoom);
    }
    g_page_y[n] = y;
    g_doc_total_h = y;

    /* Resize the spacer to drive the scroll area's content height to
     * the full document height — this is what makes the scrollbar
     * reflect the global position. */
    if (g_spacer) {
        lv_obj_set_size(g_spacer, 1, g_doc_total_h);
        lv_obj_set_pos(g_spacer, 0, 0);
    }
}

static void position_window_images(void)
{
    if (!g_page_y || !g_img) return;
    int n = pdf_view_page_count(g_view);
    if (n <= 0) return;
    if (g_cur_page < 0) g_cur_page = 0;
    if (g_cur_page >= n) g_cur_page = n - 1;

    /* Place the top image at its absolute Y inside the scroll content. */
    lv_obj_set_pos(g_img, 0, g_page_y[g_cur_page]);

    if (g_cur_page + 1 < n) {
        lv_obj_set_pos(g_img_next, 0, g_page_y[g_cur_page + 1]);
    }
}

static void render_two_page_window(int top_page_idx)
{
    if (!g_view) return;

    int page_count = pdf_view_page_count(g_view);
    if (page_count <= 0) return;
    if (top_page_idx < 0) top_page_idx = 0;
    if (top_page_idx >= page_count) top_page_idx = page_count - 1;

    g_cur_page = top_page_idx;

    const lv_image_dsc_t *d0 = pdf_view_render_page(g_view, g_cur_page, g_disp_w, g_disp_h, g_zoom);
    if (!d0) {
        printf("[ui_main] render failed (page %d): %s\n", g_cur_page, pdf_view_last_error());
        return;
    }
    lv_image_set_src(g_img, d0);
    lv_obj_set_size(g_img, (int32_t)d0->header.w, (int32_t)d0->header.h);

    if (g_cur_page + 1 < page_count) {
        const lv_image_dsc_t *d1 = pdf_view_render_page(g_view, g_cur_page + 1, g_disp_w, g_disp_h, g_zoom);
        if (d1) {
            lv_obj_clear_flag(g_img_next, LV_OBJ_FLAG_HIDDEN);
            lv_image_set_src(g_img_next, d1);
            lv_obj_set_size(g_img_next, (int32_t)d1->header.w, (int32_t)d1->header.h);
        } else {
            lv_obj_add_flag(g_img_next, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(g_img_next, LV_OBJ_FLAG_HIDDEN);
    }

    position_window_images();
    update_nav_labels();

    pdf_view_prefetch(g_view, g_cur_page, g_disp_w, g_disp_h, g_zoom);
    if (g_cur_page + 1 < page_count) {
        pdf_view_prefetch(g_view, g_cur_page + 1, g_disp_w, g_disp_h, g_zoom);
    }
}

static void render_current_page(void)
{
    render_two_page_window(g_cur_page);
    if (g_page_y) {
        lv_obj_scroll_to_y(g_scroll_area, g_page_y[g_cur_page], LV_ANIM_OFF);
    }
}

static void update_nav_labels(void)
{
    char buf[64];

    /* Page indicator */
    snprintf(buf, sizeof(buf), "%d / %d",
             g_cur_page + 1, pdf_view_page_count(g_view));
    lv_label_set_text(g_lbl_page, buf);

    /* Zoom indicator */
    snprintf(buf, sizeof(buf), "%.0f%%", g_zoom * 100.0f);
    lv_label_set_text(g_lbl_zoom, buf);

    /* Enable/disable nav buttons */
    lv_obj_set_state(g_btn_prev,
        g_cur_page <= 0 ? LV_STATE_DISABLED : LV_STATE_DEFAULT, true);
    lv_obj_set_state(g_btn_next,
        g_cur_page >= pdf_view_page_count(g_view) - 1 ?
            LV_STATE_DISABLED : LV_STATE_DEFAULT, true);
    lv_obj_set_state(g_btn_zoom_in,
        g_zoom >= ZOOM_MAX ? LV_STATE_DISABLED : LV_STATE_DEFAULT, true);
    lv_obj_set_state(g_btn_zoom_out,
        g_zoom <= ZOOM_MIN ? LV_STATE_DISABLED : LV_STATE_DEFAULT, true);
}

static lv_obj_t *make_nav_btn(lv_obj_t *parent, const char *label,
                               lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, 40);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_center(lbl);
    return btn;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

void ui_main_create(pdf_view_t *view, int disp_w, int disp_h)
{
    g_view   = view;
    g_disp_w = disp_w;
    g_disp_h = disp_h;

    /* ---- Root screen ---- */
    g_screen = lv_obj_create(NULL);
    lv_obj_set_size(g_screen, disp_w, disp_h);
    lv_obj_set_style_bg_color(g_screen, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_pad_all(g_screen, 0, 0);

    /* ---- Top bar ---- */
    g_topbar = lv_obj_create(g_screen);
    lv_obj_set_size(g_topbar, disp_w, TOPBAR_H);
    lv_obj_align(g_topbar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(g_topbar, lv_color_hex(0x2D2D2D), 0);
    lv_obj_set_style_border_width(g_topbar, 0, 0);
    lv_obj_set_style_radius(g_topbar, 0, 0);
    lv_obj_set_style_pad_hor(g_topbar, 12, 0);
    lv_obj_set_flex_flow(g_topbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_topbar,
        LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_topbar, LV_OBJ_FLAG_SCROLLABLE);

    g_lbl_title = lv_label_create(g_topbar);
    lv_label_set_text(g_lbl_title, "PDF Viewer");
    lv_label_set_long_mode(g_lbl_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(g_lbl_title, disp_w / 2);
    lv_obj_set_style_text_color(g_lbl_title, lv_color_hex(0xFFFFFF), 0);

    g_lbl_page = lv_label_create(g_topbar);
    lv_label_set_text(g_lbl_page, "0 / 0");
    lv_obj_set_style_text_color(g_lbl_page, lv_color_hex(0xAAAAAA), 0);
    lv_obj_add_flag(g_lbl_page, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_lbl_page, cb_page_label_clicked, LV_EVENT_CLICKED, NULL);

    /* Search button (right edge of topbar) */
    g_btn_search = lv_button_create(g_topbar);
    lv_obj_set_size(g_btn_search, 48, 36);
    lv_obj_set_style_bg_color(g_btn_search, lv_color_hex(0x404040), 0);
    lv_obj_set_style_radius(g_btn_search, 6, 0);
    lv_obj_add_event_cb(g_btn_search, cb_btn_search_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_s = lv_label_create(g_btn_search);
    lv_label_set_text(lbl_s, "Find");
    lv_obj_set_style_text_color(lbl_s, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_s);

    /* ---- Scroll area (holds the page image) ---- */
    int scroll_h = disp_h - TOPBAR_H - NAVBAR_H;
    g_scroll_area = lv_obj_create(g_screen);
    lv_obj_set_size(g_scroll_area, disp_w, scroll_h);
    lv_obj_align(g_scroll_area, LV_ALIGN_TOP_LEFT, 0, TOPBAR_H);
    lv_obj_set_style_bg_color(g_scroll_area, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width(g_scroll_area, 0, 0);
    lv_obj_set_style_radius(g_scroll_area, 0, 0);
    lv_obj_set_style_pad_all(g_scroll_area, 0, 0);
    /* Allow both vertical and horizontal panning after zoom-in. */
    lv_obj_set_scroll_dir(g_scroll_area, LV_DIR_ALL);
    /* Keep in-page scrolling stable: disable momentum/elastic bounce.
     * Page changes are handled explicitly by gesture + boundary overscroll logic. */
    lv_obj_clear_flag(g_scroll_area, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(g_scroll_area, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_scrollbar_mode(g_scroll_area, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_event_cb(g_scroll_area, cb_scroll, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(g_scroll_area, cb_scroll_end, LV_EVENT_SCROLL_END, NULL);

    /* Invisible spacer drives scroll-area content height to the full
     * document height so the LVGL scroll bar reflects the global
     * position naturally. */
    g_spacer = lv_obj_create(g_scroll_area);
    lv_obj_set_size(g_spacer, 1, 1);
    lv_obj_set_pos(g_spacer, 0, 0);
    lv_obj_set_style_bg_opa(g_spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_spacer, 0, 0);
    lv_obj_set_style_pad_all(g_spacer, 0, 0);
    lv_obj_clear_flag(g_spacer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_spacer, LV_OBJ_FLAG_SCROLLABLE);

    g_img = lv_image_create(g_scroll_area);
    g_img_next = lv_image_create(g_scroll_area);
    /* Let parent scroll area handle drag immediately.
     * On some touch backends, keeping image clickable can make scroll start only
     * after long-press-like behavior. */
    lv_obj_clear_flag(g_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_img, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(g_img, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(g_img_next, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_img_next, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(g_img_next, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_pos(g_img, 0, 0);
    lv_obj_set_pos(g_img_next, 0, 0);

    /* ---- Bottom nav bar ---- */
    g_navbar = lv_obj_create(g_screen);
    lv_obj_set_size(g_navbar, disp_w, NAVBAR_H);
    lv_obj_align(g_navbar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(g_navbar, lv_color_hex(0x2D2D2D), 0);
    lv_obj_set_style_border_width(g_navbar, 0, 0);
    lv_obj_set_style_radius(g_navbar, 0, 0);
    lv_obj_set_style_pad_hor(g_navbar, 12, 0);
    lv_obj_set_style_pad_ver(g_navbar, 6, 0);
    lv_obj_set_flex_flow(g_navbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_navbar,
        LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_navbar, LV_OBJ_FLAG_SCROLLABLE);

    g_btn_prev    = make_nav_btn(g_navbar, LV_SYMBOL_LEFT " Prev", cb_btn_prev);
    g_btn_zoom_out= make_nav_btn(g_navbar, LV_SYMBOL_MINUS,        cb_btn_zoom_out);

    g_lbl_zoom = lv_label_create(g_navbar);
    lv_label_set_text(g_lbl_zoom, "100%");
    lv_obj_set_style_text_color(g_lbl_zoom, lv_color_hex(0xFFFFFF), 0);

    g_btn_zoom_in = make_nav_btn(g_navbar, LV_SYMBOL_PLUS,    cb_btn_zoom_in);
    g_btn_next    = make_nav_btn(g_navbar, "Next " LV_SYMBOL_RIGHT, cb_btn_next);

    /* ---- Load screen and render first page ---- */
    lv_screen_load(g_screen);
    rebuild_page_offsets();
    render_current_page();
}

void ui_main_goto_page(int page_idx)
{
    if (!g_view) return;
    int page_count = pdf_view_page_count(g_view);
    if (page_count <= 0) return;

    if (page_idx < 0) page_idx = 0;
    if (page_idx >= page_count) page_idx = page_count - 1;

    g_cur_page = page_idx;
    render_current_page();
}

bool ui_main_goto_page_number(int page_no_1based)
{
    if (!g_view) return false;

    int page_count = pdf_view_page_count(g_view);
    if (page_count <= 0) return false;
    if (page_no_1based < 1 || page_no_1based > page_count) return false;

    ui_main_goto_page(page_no_1based - 1);
    return true;
}

int ui_main_current_page_number(void)
{
    return g_cur_page + 1;
}

int ui_main_total_pages(void)
{
    if (!g_view) return 0;
    return pdf_view_page_count(g_view);
}

void ui_main_zoom(float delta)
{
    float new_zoom = g_zoom + delta;
    if (new_zoom < ZOOM_MIN) new_zoom = ZOOM_MIN;
    if (new_zoom > ZOOM_MAX) new_zoom = ZOOM_MAX;
    if (new_zoom == g_zoom) return;

    g_zoom = new_zoom;
    pdf_view_cache_clear(g_view);   /* old zoom renders invalid */
    rebuild_page_offsets();         /* heights changed */
    render_current_page();
}

/* -------------------------------------------------------------------------- */
/* Page-jump dialog                                                            */
/* -------------------------------------------------------------------------- */

static void close_page_jump_dialog(void)
{
    if (g_jump_kb) {
        lv_obj_del(g_jump_kb);
        g_jump_kb = NULL;
    }
    if (g_jump_modal) {
        lv_obj_del(g_jump_modal);
        g_jump_modal = NULL;
        g_jump_input = NULL;
        g_jump_status = NULL;
    }
}

static void cb_jump_cancel(lv_event_t *e)
{
    (void)e;
    close_page_jump_dialog();
}

static void cb_jump_confirm(lv_event_t *e)
{
    (void)e;
    if (!g_jump_input) return;
    const char *txt = lv_textarea_get_text(g_jump_input);
    if (!txt || !*txt) {
        if (g_jump_status) lv_label_set_text(g_jump_status, "Please enter a page number");
        return;
    }
    int n = atoi(txt);
    int total = ui_main_total_pages();
    if (n < 1 || n > total) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Out of range (1..%d)", total);
        if (g_jump_status) lv_label_set_text(g_jump_status, buf);
        return;
    }
    if (ui_main_goto_page_number(n)) {
        close_page_jump_dialog();
    } else {
        if (g_jump_status) lv_label_set_text(g_jump_status, "Jump failed");
    }
}

static void show_page_jump_dialog(void)
{
    if (g_jump_modal) return;       /* already open */
    if (!g_screen) return;

    /* Modal backdrop */
    g_jump_modal = lv_obj_create(g_screen);
    lv_obj_remove_style_all(g_jump_modal);
    lv_obj_set_size(g_jump_modal, g_disp_w, g_disp_h);
    lv_obj_align(g_jump_modal, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(g_jump_modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_jump_modal, LV_OPA_60, 0);
    lv_obj_clear_flag(g_jump_modal, LV_OBJ_FLAG_SCROLLABLE);

    /* Card */
    lv_obj_t *card = lv_obj_create(g_jump_modal);
    int card_w = (g_disp_w < 480) ? g_disp_w - 40 : 360;
    lv_obj_set_size(card, card_w, LV_SIZE_CONTENT);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2D2D2D), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Title */
    lv_obj_t *title = lv_label_create(card);
    char tbuf[64];
    snprintf(tbuf, sizeof(tbuf), "Go to page (1..%d)", ui_main_total_pages());
    lv_label_set_text(title, tbuf);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    /* Input */
    g_jump_input = lv_textarea_create(card);
    lv_textarea_set_one_line(g_jump_input, true);
    lv_textarea_set_accepted_chars(g_jump_input, "0123456789");
    lv_textarea_set_max_length(g_jump_input, 8);
    lv_textarea_set_placeholder_text(g_jump_input, "page #");
    lv_obj_set_width(g_jump_input, lv_pct(90));

    /* Status */
    g_jump_status = lv_label_create(card);
    lv_label_set_text(g_jump_status, "");
    lv_obj_set_style_text_color(g_jump_status, lv_color_hex(0xFF8080), 0);

    /* Buttons row */
    lv_obj_t *row = lv_obj_create(card);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
        LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_cancel = lv_button_create(row);
    lv_obj_set_size(btn_cancel, LV_SIZE_CONTENT, 40);
    lv_obj_add_event_cb(btn_cancel, cb_jump_cancel, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_c = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_c, "Cancel");
    lv_obj_center(lbl_c);

    lv_obj_t *btn_ok = lv_button_create(row);
    lv_obj_set_size(btn_ok, LV_SIZE_CONTENT, 40);
    lv_obj_add_event_cb(btn_ok, cb_jump_confirm, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "Go");
    lv_obj_center(lbl_ok);

    /* Number keyboard */
    g_jump_kb = lv_keyboard_create(g_jump_modal);
    lv_keyboard_set_mode(g_jump_kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(g_jump_kb, g_jump_input);
    lv_obj_set_size(g_jump_kb, g_disp_w, g_disp_h / 3);
    lv_obj_align(g_jump_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
}

static void cb_page_label_clicked(lv_event_t *e)
{
    (void)e;
    show_page_jump_dialog();
}

/* -------------------------------------------------------------------------- */
/* Text search dialog                                                          */
/* -------------------------------------------------------------------------- */

static void search_clear_results(void)
{
    if (g_search_hits) {
        pdf_search_result_free(g_search_hits);
        g_search_hits = NULL;
    }
    g_search_cursor = -1;
}

static void search_update_status(const char *override)
{
    if (!g_search_status) return;
    if (override) { lv_label_set_text(g_search_status, override); return; }
    if (!g_search_hits || g_search_hits->count == 0) {
        lv_label_set_text(g_search_status, "No matches");
        return;
    }
    char buf[96];
    int cur = (g_search_cursor >= 0) ? (g_search_cursor + 1) : 0;
    snprintf(buf, sizeof(buf), "%d / %d%s  (page %d)",
             cur, g_search_hits->count,
             g_search_hits->truncated ? "+" : "",
             g_search_hits->hits[g_search_cursor < 0 ? 0 : g_search_cursor].page + 1);
    lv_label_set_text(g_search_status, buf);
}

static void search_jump_to_cursor(void)
{
    if (!g_search_hits || g_search_cursor < 0 ||
        g_search_cursor >= g_search_hits->count) return;
    pdf_search_hit_t *h = &g_search_hits->hits[g_search_cursor];
    /* Land the page at the top of the viewport. */
    ui_main_goto_page_number(h->page + 1);

    /* Then refine: scroll the hit roughly into the upper third using
     * absolute Y = page_top + hit_y_in_page. */
    if (!g_page_y || !g_view) { search_update_status(NULL); return; }
    float pw = 0, ph = 0;
    if (pdf_view_page_size(g_view, h->page, &pw, &ph) && ph > 0) {
        int page_h_px = page_height_for_zoom(h->page, g_zoom);
        int hit_y_in_page = (int)((h->y / ph) * page_h_px);
        int abs_y = g_page_y[h->page] + hit_y_in_page;
        int viewport_h = lv_obj_get_content_height(g_scroll_area);
        int target = abs_y - viewport_h / 3;
        if (target < 0) target = 0;
        lv_obj_scroll_to_y(g_scroll_area, target, LV_ANIM_ON);
    }
    search_update_status(NULL);
}

static void cb_search_run(lv_event_t *e)
{
    (void)e;
    if (!g_search_input || !g_view) return;
    const char *txt = lv_textarea_get_text(g_search_input);
    if (!txt || !*txt) {
        search_update_status("Enter a query");
        return;
    }
    /* Cap max hits to 1000 to keep memory bounded. */
    search_clear_results();
    strncpy(g_search_query, txt, sizeof(g_search_query) - 1);
    g_search_query[sizeof(g_search_query) - 1] = '\0';
    g_search_hits = pdf_view_search(g_view, txt, 0, false, 1000);
    if (!g_search_hits) {
        search_update_status(pdf_view_last_error());
        return;
    }
    if (g_search_hits->count == 0) {
        search_update_status("No matches");
        return;
    }
    g_search_cursor = 0;
    search_jump_to_cursor();
}

static void cb_search_next(lv_event_t *e)
{
    (void)e;
    if (!g_search_hits || g_search_hits->count == 0) return;
    g_search_cursor = (g_search_cursor + 1) % g_search_hits->count;
    search_jump_to_cursor();
}

static void cb_search_prev(lv_event_t *e)
{
    (void)e;
    if (!g_search_hits || g_search_hits->count == 0) return;
    g_search_cursor = (g_search_cursor - 1 + g_search_hits->count)
                      % g_search_hits->count;
    search_jump_to_cursor();
}

static void close_search_dialog(void)
{
    if (g_search_kb) { lv_obj_del(g_search_kb); g_search_kb = NULL; }
    if (g_search_modal) {
        lv_obj_del(g_search_modal);
        g_search_modal = NULL;
        g_search_input = NULL;
        g_search_status = NULL;
    }
}

static void cb_search_close(lv_event_t *e)
{
    (void)e;
    close_search_dialog();
}

static void cb_btn_search_clicked(lv_event_t *e)
{
    (void)e;
    show_search_dialog();
}

static void show_search_dialog(void)
{
    if (g_search_modal) return;
    if (!g_screen) return;

    g_search_modal = lv_obj_create(g_screen);
    lv_obj_remove_style_all(g_search_modal);
    lv_obj_set_size(g_search_modal, g_disp_w, g_disp_h);
    lv_obj_align(g_search_modal, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(g_search_modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_search_modal, LV_OPA_60, 0);
    lv_obj_clear_flag(g_search_modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card = lv_obj_create(g_search_modal);
    int card_w = (g_disp_w < 520) ? g_disp_w - 40 : 460;
    lv_obj_set_size(card, card_w, LV_SIZE_CONTENT);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2D2D2D), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "Find in document");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    g_search_input = lv_textarea_create(card);
    lv_textarea_set_one_line(g_search_input, true);
    lv_textarea_set_max_length(g_search_input, 120);
    lv_textarea_set_placeholder_text(g_search_input, "search text");
    if (g_search_query[0]) lv_textarea_set_text(g_search_input, g_search_query);
    lv_obj_set_width(g_search_input, lv_pct(95));

    g_search_status = lv_label_create(card);
    lv_label_set_text(g_search_status,
                      g_search_hits ? "" : "Type and tap Search");
    lv_obj_set_style_text_color(g_search_status, lv_color_hex(0xFFCC66), 0);
    if (g_search_hits) search_update_status(NULL);

    /* Buttons row */
    lv_obj_t *row = lv_obj_create(card);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
        LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_close = lv_button_create(row);
    lv_obj_set_size(btn_close, LV_SIZE_CONTENT, 40);
    lv_obj_add_event_cb(btn_close, cb_search_close, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lc = lv_label_create(btn_close);
    lv_label_set_text(lc, "Close"); lv_obj_center(lc);

    lv_obj_t *btn_prev = lv_button_create(row);
    lv_obj_set_size(btn_prev, LV_SIZE_CONTENT, 40);
    lv_obj_add_event_cb(btn_prev, cb_search_prev, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lp = lv_label_create(btn_prev);
    lv_label_set_text(lp, LV_SYMBOL_UP " Prev"); lv_obj_center(lp);

    lv_obj_t *btn_next = lv_button_create(row);
    lv_obj_set_size(btn_next, LV_SIZE_CONTENT, 40);
    lv_obj_add_event_cb(btn_next, cb_search_next, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ln = lv_label_create(btn_next);
    lv_label_set_text(ln, "Next " LV_SYMBOL_DOWN); lv_obj_center(ln);

    lv_obj_t *btn_run = lv_button_create(row);
    lv_obj_set_size(btn_run, LV_SIZE_CONTENT, 40);
    lv_obj_add_event_cb(btn_run, cb_search_run, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lr = lv_label_create(btn_run);
    lv_label_set_text(lr, "Search"); lv_obj_center(lr);

    /* Text keyboard */
    g_search_kb = lv_keyboard_create(g_search_modal);
    lv_keyboard_set_mode(g_search_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(g_search_kb, g_search_input);
    lv_obj_set_size(g_search_kb, g_disp_w, g_disp_h * 2 / 5);
    lv_obj_align(g_search_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
}
