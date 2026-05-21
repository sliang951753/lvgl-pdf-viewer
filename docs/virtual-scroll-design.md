# Virtual full-document scroll model

## Problem

The original implementation reflowed a 2-page window inside the scroll
container. Two issues followed:

1. **Scrollbar lied.** The container only ever held 2 pages of content,
   so the LVGL scrollbar showed "we are at the bottom" the moment the
   user scrolled past the second page, regardless of which two pages
   were currently mounted.
2. **Could not scroll back.** With `LV_OBJ_FLAG_SCROLL_ELASTIC` and
   momentum disabled (needed for deterministic reading), `y == 0` no
   longer emitted a gesture event, so an upward drag at the top of the
   window had no way to swap the window back to (N-1, N).

## Approach

Make the scroll container's content size equal the size of the entire
document, and lay each page at its absolute Y inside that virtual
canvas. Native LVGL scroll then "just works" — including the scrollbar
and scrolling back to earlier pages — without any gesture plumbing.

```
┌─ g_scroll_area  (visible viewport: g_disp_w × g_disp_h) ────────┐
│  ┌─ g_spacer  (hidden, size = 1 × g_doc_total_h) ─────────────┐ │
│  │   y=0        ┌──── g_img_a  (page N at g_page_y[N]) ────┐  │ │
│  │              │                                          │  │ │
│  │              └──────────────────────────────────────────┘  │ │
│  │   y=...      ┌──── g_img_b  (page N+1 at g_page_y[N+1]) ─┐  │ │
│  │              │                                           │  │ │
│  │              └───────────────────────────────────────────┘  │ │
│  │   ...                                                       │ │
│  │   y=g_doc_total_h                                           │ │
│  └─────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

## Data

```c
static int      g_page_y[N_PAGES + 1];   // absolute Y per page, last entry = total height (sentinel)
static int      g_doc_total_h;
static lv_obj_t *g_spacer;               // hidden, sized 1 × g_doc_total_h
static lv_obj_t *g_img_a, *g_img_b;      // two recycled lv_image widgets
```

`rebuild_page_offsets()` walks every page once, calling
`pdf_view_page_size()` (no render) and the current zoom, and fills
`g_page_y[]` and `g_doc_total_h`. Run on open and on every zoom change.

## Reflow

`cb_scroll` runs binary search over `g_page_y[]` to find the visible
page from `lv_obj_get_scroll_y()` and calls `position_window_images()`
to place `g_img_a` (current page) and `g_img_b` (next page, if any) at
their absolute Ys via `lv_obj_set_pos`. A re-entrancy guard
(`g_window_reflow_guard`) prevents `cb_scroll` from firing recursively
while we move children.

## Centering after zoom (LVGL 9 pitfall)

`lv_obj_set_size(img, w, h)` updates the descriptor immediately but the
**coord cache** that `lv_obj_get_width(img)` reads is only refreshed at
the next layout pass. So computing `centered_x = (viewport - get_width)
/ 2` right after a zoom uses the *previous* page width and leaves the
page off-center.

Two fixes, applied together:

1. `centered_x_for(int32_t img_w)` accepts the width as an argument.
   Caller passes the width it just set (`pix->header.w` from
   `lv_image_dsc_t`).
2. After zoom: `render → lv_obj_update_layout(g_scroll_area) →
   lv_obj_scroll_to_x(0)` so the container's content_width is up to
   date before snapping horizontally.

Same pitfall applies to `lv_obj_set_state(obj, LV_STATE_DEFAULT, true)`
— `LV_STATE_DEFAULT` is `0` and the API translates to
`lv_obj_add_state(obj, 0)` which clears nothing. To re-enable a button
that was previously disabled, use `lv_obj_remove_state(obj,
LV_STATE_DISABLED)` explicitly.

## Search integration

`pdf_view_search` returns hits in document order with axis-aligned
bboxes (page index + `{x, y, w, h}` in rendered pixels at current
zoom). Search jump scrolls to absolute Y:

```c
int32_t y = g_page_y[hit.page] + hit.bbox.y;
lv_obj_scroll_to_y(g_scroll_area, y, LV_ANIM_OFF);
```

Hit overlay (red rect) is planned but not yet implemented.
