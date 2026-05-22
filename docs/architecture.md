# Architecture overview

A single document that ties together the design notes living under
`docs/`. Read this first; then drill into the linked files for the
finer-grained context.

## Layered view

```
            ┌──────────────────────────────────────────────┐
            │  main.c                                       │
            │     CLI args → platform init → ui init        │
            └─────────────┬────────────────────────────────┘
                          │
        ┌─────────────────┴──────────────────────┐
        │                                        │
┌───────▼─────────┐                    ┌─────────▼─────────┐
│ platform/        │                   │ ui/                │
│   .h HAL          │                  │   ui_main.c        │
│   _win32.c        │                  │   - top bar         │
│   _sdl2.c         │                  │   - scroll area     │
│                   │                  │   - dialogs         │
│ display + tick    │                  │   - search overlay  │
│ + event pump      │                  └─────────┬───────────┘
└───────────────────┘                            │
                                                 │ pdf_view API
                                       ┌─────────▼───────────┐
                                       │ pdf/                 │
                                       │   pdf_view.c/h       │
                                       │   - MuPDF wrapper    │
                                       │   - LRU page cache   │
                                       │   - text search      │
                                       └──────────────────────┘
```

LVGL ↔ MuPDF flow per page:

```
fz_load_page → fz_run_page (display list) → fz_pixmap (ARGB)
            → lv_image_dsc_t (zero-copy)  → lv_image widget
            → LVGL SW renderer → platform display driver
```

## Module responsibilities

| Module | Responsibility | Key entry points |
|---|---|---|
| `main.c` | parse args, init systems, run event loop | `WinMain` / `main` |
| `platform/` | abstract display + tick + quit signal for Win32/SDL2 | `platform_init`, `platform_tick`, `platform_should_quit` |
| `pdf/pdf_view` | MuPDF context, page render, LRU cache, page-size query, text search | `pdf_view_open/close/render_page/page_size/search` |
| `ui/ui_main` | LVGL screen layout, virtual scroll, dialogs, search overlay | `ui_main_init`, `ui_main_goto_page_number` |

## Display + scroll model (virtual full-doc)

The scroll container's content height equals the **whole document
height in pixels** at the current zoom. Each page is positioned
absolutely (`lv_obj_set_pos(img, x, g_page_y[N])`). Two `lv_image`
widgets are recycled to render the current and next visible pages.

```
g_scroll_area  (viewport)
 └── g_spacer   (1 × g_doc_total_h, drives content size only)
 └── g_img      (current page, child of g_scroll_area at g_page_y[N])
 └── g_img_next (next page,    child of g_scroll_area at g_page_y[N+1])
 └── g_search_rects[] (overlay rectangles, child of g_scroll_area too)
```

Details: `docs/virtual-scroll-design.md`.

> **Pitfall**: any overlay positioned by absolute Y *must* be a child
> of `g_scroll_area`, **not** of `g_spacer`. The spacer is sized 1×1
> for content driving and would clip the overlay to a single pixel.

The continuous-scroll behaviour (no hard page flip) is documented in
`docs/continuous-scroll-design.md`. Note that the original "two-page
window with residual offset" approach was superseded by the virtual
full-doc model — the continuous-scroll file is kept for historical
context and for the rationale of choosing Word-like flow over gesture
flip.

## Text search pipeline

```
pdf_view_search(view, needle, start_page, wrap, max_hits)
   │
   ├── per page: fz_search_page_number → hit_bbox[]
   ├── accumulate up to max_hits, set truncated flag
   ▼
pdf_search_result_t { hits[], count, truncated }
   │
   ▼ UI side
ui_main applies current zoom scale:
   img_y = g_page_y[hit.page] + hit.y_points * scale_y
   img_x = hit.x_points * scale_x
draw overlay rect, active hit gets thicker border + translucent fill
```

Search hit coordinates from MuPDF are in **PDF points** (1pt = 1/72in)
with origin at top-left. The UI converts them to rendered-pixel space
using `pdf_view_page_size()` and the active zoom factor.

`pdf_view_search_page()` is a per-page wrapper used by the overlay to
refresh hits when the user scrolls into a new page without re-running
the full document scan.

## State diagram (UI)

```
        open file
           │
           ▼
       [Reading] ──── Search button ───→ [Search dialog]
        ▲    │                              │ Find
        │    │                              ▼
        │    │                          [Search active]
        │    │  Clear / Close ◀─────────  ▲ │ Prev/Next
        │    │                            │ │ (cursor moves)
        │    │  Zoom +/- ──────────────── rebuild offsets,
        │    │                            re-render, refresh overlay
        │    │  Prev/Next page ────────── scroll_to(g_page_y[N±1])
        │    │  Click page indicator ──→ [Page jump dialog]
        │    │                            │ Go
        │    ▼                            ▼
        └─── scroll_to(g_page_y[target]) ──────────────────────────
```

## LVGL 9 pitfalls encountered (memorialise these)

1. `lv_obj_set_state(obj, LV_STATE_DEFAULT, true)` is a **no-op**.
   `LV_STATE_DEFAULT == 0`, and `add_state(0)` clears nothing.
   Use `lv_obj_add_state(obj, LV_STATE_DISABLED)` /
   `lv_obj_remove_state(obj, LV_STATE_DISABLED)` explicitly.
2. `lv_obj_get_width(obj)` right after `lv_obj_set_size(obj, w, h)`
   returns the **stale** cached width — the coord cache only
   refreshes at the next layout pass. Either pass the width you just
   set down to the centering math, or call
   `lv_obj_update_layout(parent)` before reading.
3. Children of a 1×1 spacer get clipped to 1×1 unless you also disable
   the parent's clip flag; safer to parent overlays to the same
   scroll container the page images use.

## Testing strategy

- **Headless** (`tests/test_search_cli.c`, `tests/test_pageapi_cli.c`):
  pure C drivers linked against `pdf_view.c` + LVGL types only. No
  display server, no SDL/Win32 init. Runs in CI / vanilla shells.
- **GUI smoke** (`scripts/test_continuous_scroll_regression.sh`):
  launches the full viewer with `DISPLAY=:0`, opens a PDF, kills
  after a timeout, then grep-checks the log for expected /
  unexpected render lines.
- **Replay** (`scripts/replay_continuous_scroll_regression.sh`): same
  as the smoke test but also feeds wheel events via `xdotool` to
  exercise the scroll-back path.
- **Aggregator** (`scripts/run_all_tests.sh`): builds everything once
  and runs all of the above; non-zero exit on any failure.

See `README.md` § "Build" for invocation.
