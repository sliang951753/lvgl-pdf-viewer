# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added
- **Virtual full-document scroll model**: a hidden `g_spacer` of size `1 × total_doc_height_px` makes the scroll container's content size match the entire document, so the native LVGL scrollbar reflects whole-document position. Two `lv_image` widgets are positioned absolutely inside the spacer (`lv_obj_set_pos`) at the precomputed Y of the current and next page, and reflow as the user scrolls. Per-page Y offsets are cached in `g_page_y[]`, computed from `pdf_view_page_size()` without rendering.
- `pdf_view_page_size(view, idx, *w_pts, *h_pts)`: cheap page-size query used to build the offset table.
- **Text search**:
  - `pdf_view_search(view, needle, start_page, wrap_around, max_hits) → pdf_search_result_t` using MuPDF `fz_search_page_number` (axis-aligned bbox, document order).
  - Search dialog wired in the top bar with Prev/Next/Close hit navigation; jumps to absolute Y `g_page_y[hit.page] + hit.bbox.y` so the hit lands at the top of the viewport.
  - Defaults: `start_page=0`, `wrap_around=false`, `max_hits=1000`.
- `docs/virtual-scroll-design.md`: design notes for the virtual scroll model and zoom recentering.
- Page-jump API/UI (`ui_main_goto_page_number`, `ui_main_current_page_number`, `ui_main_total_pages`) — clickable page indicator opens a modal numeric pad with range validation.
- Continuous-scroll regression scripts (`test_continuous_scroll_regression.sh`, `replay_continuous_scroll_regression.sh`, `analyze_scroll_log.py`).

### Changed
- Replaced the previous 2-page reflow window with the virtual full-document model. Native LVGL vertical scroll now handles up/down, including scrolling back to earlier pages — the gesture callback for page flipping has been removed.
- `cb_scroll` uses binary search over `g_page_y[]` to find the current page from the scroll offset.
- `cb_btn_prev/next` now do `lv_obj_scroll_to_y(g_page_y[target])` instead of swapping the window.

### Fixed
- **Zoom recenter**: after zoom, pages stayed left-aligned because `lv_obj_get_width` right after `lv_obj_set_size` returns the cached (pre-layout) width in LVGL 9. Fix: derive the page width from `lv_image_dsc_t->header.w` (set at the same call), use `g_disp_w` as the viewport width, and order zoom as `render → lv_obj_update_layout → lv_obj_scroll_to_x(0)`.
- **Nav buttons stuck DISABLED**: `lv_obj_set_state(btn, LV_STATE_DEFAULT, true)` is a no-op in LVGL 9 (`LV_STATE_DEFAULT == 0`, `add_state(0)` clears nothing). Once a button became `LV_STATE_DISABLED` (e.g. zoom-in at MAX, zoom-out at MIN, or prev/next at document edges), the flag stuck and the button could not be re-enabled. Fix: use explicit `lv_obj_add_state` / `lv_obj_remove_state(LV_STATE_DISABLED)` for prev/next/zoom_in/zoom_out.
- Search start page corrected from page 52 to page 0 with `wrap_around=false` so hits return in document order.
- Last-page boundary off-by-one: scroll target index uses `page_count - 1` (not `page_count - 2`).

## [0.1.2] - 2026-05-21

### Fixed
- Termux:X11 touch input reliability on `linux-host`:
  - `platform_should_quit()` no longer drains SDL event queue.
  - Use `SDL_PeepEvents(...SDL_PEEKEVENT...)` to check quit state without stealing input events from LVGL SDL driver.
- Scroll interaction in PDF page area:
  - Explicitly sync `lv_image` object size to rendered page dimensions.
  - Prevent image widget from intercepting drag start by clearing clickable flag and enabling event/gesture bubbling.

### Added
- `scripts/run_gui_termux_x11.sh`: one-command local GUI launch for Termux:X11.
- `docs/termux-x11-touch-debug.md`: design notes and root-cause analysis for touch/scroll fixes.

### Docs
- README updated with Termux:X11 local simulation steps and project tree updates.
- `.gitignore` updated to ignore runtime logs under `logs/`.

## [0.1.1] - 2026-05-21

### Fixed
- Stabilized `linux-host` build on Termux/Android toolchain path.
- SDL2 integration and build configuration cleanup for local host builds.

[Unreleased]: https://github.com/sliang951753/lvgl-pdf-viewer/compare/bfc9e2d...HEAD
[0.1.2]: https://github.com/sliang951753/lvgl-pdf-viewer/compare/359a736...bfc9e2d
[0.1.1]: https://github.com/sliang951753/lvgl-pdf-viewer/commit/359a736
