# Continuous Scroll Design (Word-like Page Flow)

## Goal
Provide document-style reading flow:
- Scroll vertically inside current page.
- When the current page scrolls out of viewport, the next page appears naturally.
- No gesture-triggered hard flip in normal reading mode.

## Core Model

UI keeps a **two-page window** in one scroll container:
- top page: `N`
- next page: `N+1`

When `scroll_y >= height(N)`, reflow window to:
- top page: `N+1`
- next page: `N+2`

And preserve residual offset:
- `residual = scroll_y - height(N)`
- apply `scroll_to_y(residual)` after reflow

This makes page transitions continuous, like word/document viewers.

## Implementation (src/ui/ui_main.c)

### Key state
- `g_cur_page`: top page index in the two-page window
- `g_img`: current/top page widget
- `g_img_next`: next page widget
- `g_window_reflow_guard`: prevents recursive reflow during programmatic scroll update

### Key functions
- `render_two_page_window(int top_page_idx)`
  - renders page `N` into `g_img`
  - renders page `N+1` into `g_img_next` (if available)
  - aligns `g_img_next` under `g_img`
- `cb_scroll(lv_event_t *e)`
  - active at `zoom <= 1.0`
  - when `scroll_y >= page_height(N)`, shifts window and keeps residual
- `cb_gesture(lv_event_t *e)`
  - intentionally no-op for continuous mode (avoid duplicate/skip triggers)

## Behavioral constraints
- Zoomed mode (`zoom > 1.0`): prioritize pan semantics; no continuous-window reflow.
- Last window clamp:
  - for page count `P >= 2`, valid top index is `[0, P-2]`
  - single-page PDFs degrade safely to one page.

## Why this avoids past issues
- No dependency on unstable gesture direction recognition for page turn.
- No hard boundary “flip” at bottom of page.
- No double-trigger from gesture + scroll-end competing paths.

## Performance note
- Reflow path relies on existing MuPDF LRU cache + prefetch.
- Adjacent page rendering (`N+1`, `N+2`) tends to hit warm cache in continuous reading.
