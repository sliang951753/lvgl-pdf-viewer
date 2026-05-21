# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

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
