# Changelog

All notable changes to this project will be documented in this file.

## [0.2.0] - 2026-05-22

Milestone "M3 complete": virtual full-document scroll, page jump, zoom
recentering, text search end-to-end (API + UI + overlay + Clear), full
headless regression coverage.

### Added
- **Virtual full-document scroll model**: a hidden `g_spacer` of size
  `1 × total_doc_height_px` makes the scroll container's content size
  match the entire document, so the native LVGL scrollbar reflects
  whole-document position. Two `lv_image` widgets are positioned
  absolutely (`lv_obj_set_pos`) at the precomputed Y of the current
  and next page, and reflow as the user scrolls. Per-page Y offsets
  are cached in `g_page_y[]`, computed from `pdf_view_page_size()`
  without rendering.
- `pdf_view_page_size(view, idx, *w_pts, *h_pts)`: cheap page-size
  query used to build the offset table.
- `pdf_view_search_page(view, idx, needle, max_hits)`: per-page search
  helper used by the overlay to refresh hits without a full doc scan.
- **Text search end-to-end**:
  - `pdf_view_search()` using MuPDF `fz_search_page_number`
    (axis-aligned bbox, document order).
  - Search dialog wired in the top bar with Prev / Next / Clear /
    Close hit navigation; jumps to absolute Y
    `g_page_y[hit.page] + hit.bbox.y` so the hit lands at the top of
    the viewport.
  - **Search hit overlay**: red bbox rectangles drawn on visible
    pages; the active hit (cursor) gets a thicker border +
    translucent red fill. Overlay rectangles are children of
    `g_scroll_area` (not `g_spacer`, which is a 1×1 sentinel) and
    share the virtual-scroll coordinate system.
- **Page-jump dialog**: clickable page indicator opens a modal numeric
  pad with range validation (`ui_main_goto_page_number`,
  `ui_main_current_page_number`, `ui_main_total_pages`).
- **Headless API regression** (`tests/test_pageapi_cli.c` +
  `scripts/test_pageapi_regression.sh`): 7 checks × 3 PDFs covering
  `pdf_view_page_count`, `pdf_view_page_size` (positive + out-of-range
  + negative index), `pdf_view_render_page`, `pdf_view_prefetch`,
  `pdf_view_cache_clear`, and post-cache-clear re-render.
- **Headless search regression** (`tests/test_search_cli.c` +
  `scripts/test_search_regression.sh`): 5 cases — positive match,
  deeper start, no-match, truncation flag, second document.
- **GUI smoke + replay** (`test_continuous_scroll_regression.sh`,
  `replay_continuous_scroll_regression.sh`,
  `scripts/analyze_scroll_log.py`).
- **One-shot test aggregator** (`scripts/run_all_tests.sh`): builds
  `linux-host` once, runs all headless suites, and runs GUI suites
  when `DISPLAY` (and optionally `xdotool`) are available. Safe on CI.
- **Architecture overview** (`docs/architecture.md`): module map,
  display + scroll model diagram, search pipeline, LVGL-9 pitfalls,
  testing strategy.
- README testing section + `tests/` entry in the project tree.

### Changed
- Replaced the previous 2-page reflow window with the virtual
  full-document model. Native LVGL vertical scroll now handles
  up/down, including scrolling back to earlier pages — the gesture
  callback for page flipping has been removed.
- `cb_scroll` uses binary search over `g_page_y[]` to find the current
  page from the scroll offset.
- `cb_btn_prev/next` now do `lv_obj_scroll_to_y(g_page_y[target])`
  instead of swapping the window.
- README: features list moved completed search/overlay items out of
  "planned"; build section now includes the Termux:X11 client APK
  install step (previously missing — viewer launched but no GUI
  appeared because the Android-side APK was not installed).

### Fixed
- **Zoom recenter**: after zoom, pages stayed left-aligned because
  `lv_obj_get_width` right after `lv_obj_set_size` returns the cached
  (pre-layout) width in LVGL 9. Fix: derive the page width from
  `lv_image_dsc_t->header.w` (set at the same call), use `g_disp_w`
  as the viewport width, and order zoom as
  `render → lv_obj_update_layout → lv_obj_scroll_to_x(0)`.
- **Nav buttons stuck DISABLED**:
  `lv_obj_set_state(btn, LV_STATE_DEFAULT, true)` is a no-op in
  LVGL 9 (`LV_STATE_DEFAULT == 0`, `add_state(0)` clears nothing).
  Once a button became `LV_STATE_DISABLED` (e.g. zoom-in at MAX,
  zoom-out at MIN, or prev/next at document edges), the flag stuck
  and the button could not be re-enabled. Fix: explicit
  `lv_obj_add_state` / `lv_obj_remove_state(LV_STATE_DISABLED)` for
  prev / next / zoom_in / zoom_out.
- **Search hit overlay clipped to 1×1**: rectangles were initially
  parented to `g_spacer`, the 1×1 content-size sentinel. Re-parented
  to `g_scroll_area` so they share coordinates with the page images.
- Search start page corrected from page 52 to page 0 with
  `wrap_around=false` so hits return in document order.
- Last-page boundary off-by-one: scroll target index uses
  `page_count - 1` (not `page_count - 2`).

## [0.1.2] - 2026-05-21

### Fixed
- Termux:X11 touch input reliability on `linux-host`:
  - `platform_should_quit()` no longer drains SDL event queue.
  - Use `SDL_PeepEvents(...SDL_PEEKEVENT...)` to check quit state
    without stealing input events from LVGL SDL driver.
- Scroll interaction in PDF page area:
  - Explicitly sync `lv_image` object size to rendered page
    dimensions.
  - Prevent image widget from intercepting drag start by clearing
    clickable flag and enabling event/gesture bubbling.

### Added
- `scripts/run_gui_termux_x11.sh`: one-command local GUI launch for
  Termux:X11.
- `docs/termux-x11-touch-debug.md`: design notes and root-cause
  analysis for touch/scroll fixes.

### Docs
- README updated with Termux:X11 local simulation steps and project
  tree updates.
- `.gitignore` updated to ignore runtime logs under `logs/`.

## [0.1.1] - 2026-05-21

### Fixed
- Stabilized `linux-host` build on Termux/Android toolchain path.
- SDL2 integration and build configuration cleanup for local host
  builds.

[0.2.0]: https://github.com/sliang951753/lvgl-pdf-viewer/compare/bfc9e2d...HEAD
[0.1.2]: https://github.com/sliang951753/lvgl-pdf-viewer/compare/359a736...bfc9e2d
[0.1.1]: https://github.com/sliang951753/lvgl-pdf-viewer/commit/359a736
