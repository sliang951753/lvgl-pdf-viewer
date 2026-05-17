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
#include <string.h>

/* -------------------------------------------------------------------------- */
/* State                                                                       */
/* -------------------------------------------------------------------------- */

static pdf_view_t  *g_view     = NULL;
static int          g_disp_w   = 800;
static int          g_disp_h   = 480;
static int          g_cur_page = 0;
static float        g_zoom     = 1.0f;

/* LVGL objects */
static lv_obj_t *g_screen      = NULL;
static lv_obj_t *g_topbar      = NULL;
static lv_obj_t *g_lbl_title   = NULL;
static lv_obj_t *g_lbl_page    = NULL;
static lv_obj_t *g_scroll_area = NULL;
static lv_obj_t *g_img         = NULL;
static lv_obj_t *g_navbar      = NULL;
static lv_obj_t *g_btn_prev    = NULL;
static lv_obj_t *g_btn_next    = NULL;
static lv_obj_t *g_lbl_zoom    = NULL;
static lv_obj_t *g_btn_zoom_in = NULL;
static lv_obj_t *g_btn_zoom_out= NULL;

#define TOPBAR_H  48
#define NAVBAR_H  56
#define ZOOM_STEP 0.25f
#define ZOOM_MIN  0.5f
#define ZOOM_MAX  4.0f

/* -------------------------------------------------------------------------- */
/* Forward declarations                                                        */
/* -------------------------------------------------------------------------- */

static void render_current_page(void);
static void update_nav_labels(void);

/* -------------------------------------------------------------------------- */
/* Event callbacks                                                             */
/* -------------------------------------------------------------------------- */

static void cb_btn_prev(lv_event_t *e)
{
    (void)e;
    if (g_cur_page > 0) {
        g_cur_page--;
        render_current_page();
    }
}

static void cb_btn_next(lv_event_t *e)
{
    (void)e;
    if (g_cur_page < pdf_view_page_count(g_view) - 1) {
        g_cur_page++;
        render_current_page();
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

/* Swipe gesture on the scroll area for page navigation */
static void cb_gesture(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT)       cb_btn_next(e);
    else if (dir == LV_DIR_RIGHT) cb_btn_prev(e);
}

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                            */
/* -------------------------------------------------------------------------- */

static void render_current_page(void)
{
    /* Render page (may come from cache) */
    const lv_image_dsc_t *dsc = pdf_view_render_page(
        g_view, g_cur_page, g_disp_w, g_disp_h, g_zoom);

    if (dsc == NULL) {
        printf("[ui_main] render failed: %s\n", pdf_view_last_error());
        return;
    }

    /* Update LVGL image */
    lv_image_set_src(g_img, dsc);

    /* Reset scroll to top */
    lv_obj_scroll_to(g_scroll_area, 0, 0, LV_ANIM_OFF);

    /* Update labels */
    update_nav_labels();

    /* Kick off prefetch for adjacent pages */
    pdf_view_prefetch(g_view, g_cur_page, g_disp_w, g_disp_h, g_zoom);
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

    /* ---- Scroll area (holds the page image) ---- */
    int scroll_h = disp_h - TOPBAR_H - NAVBAR_H;
    g_scroll_area = lv_obj_create(g_screen);
    lv_obj_set_size(g_scroll_area, disp_w, scroll_h);
    lv_obj_align(g_scroll_area, LV_ALIGN_TOP_LEFT, 0, TOPBAR_H);
    lv_obj_set_style_bg_color(g_scroll_area, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width(g_scroll_area, 0, 0);
    lv_obj_set_style_radius(g_scroll_area, 0, 0);
    lv_obj_set_style_pad_all(g_scroll_area, 0, 0);
    lv_obj_set_scroll_dir(g_scroll_area, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_scroll_area, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_event_cb(g_scroll_area, cb_gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_flag(g_scroll_area, LV_OBJ_FLAG_GESTURE_BUBBLE);

    g_img = lv_image_create(g_scroll_area);
    lv_obj_align(g_img, LV_ALIGN_TOP_MID, 0, 0);

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
    render_current_page();
}

void ui_main_goto_page(int page_idx)
{
    if (!g_view) return;
    if (page_idx < 0) page_idx = 0;
    if (page_idx >= pdf_view_page_count(g_view))
        page_idx = pdf_view_page_count(g_view) - 1;
    g_cur_page = page_idx;
    render_current_page();
}

void ui_main_zoom(float delta)
{
    float new_zoom = g_zoom + delta;
    if (new_zoom < ZOOM_MIN) new_zoom = ZOOM_MIN;
    if (new_zoom > ZOOM_MAX) new_zoom = ZOOM_MAX;
    if (new_zoom == g_zoom) return;

    g_zoom = new_zoom;
    pdf_view_cache_clear(g_view);   /* old zoom renders invalid */
    render_current_page();
}
