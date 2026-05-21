# lvgl-pdf-viewer

A cross-platform PDF viewer built with **LVGL 9.5** and **MuPDF**, targeting:

| Platform | Display driver | Compiler |
|---|---|---|
| Windows | Win32 API (built-in LVGL driver) | MSVC (VS 2022) |
| Allwinner T507 / Timesys Linux | SDL2 | aarch64-linux-gnu-gcc |
| Linux desktop (dev) | SDL2 | GCC / Clang |

Render pipeline: `MuPDF → ARGB8888 pixmap → lv_image_dsc_t → LVGL SW renderer`

## Features (M1–M3)

- [x] Open and display local PDF files
- [x] Fit-to-width page rendering
- [x] **Virtual full-document scroll** (continuous, native scrollbar reflects whole-doc position)
- [x] Button navigation (Prev/Next) — disables correctly at document edges
- [x] Page-jump dialog (clickable page indicator → numeric pad)
- [x] Zoom in/out (25% steps, 50%–400%) — pages re-center and horizontal pan resets after zoom
- [x] **Text search** (full-document or single-page) with hit navigation (Prev/Next/Close)
- [x] LRU page cache (5 pages in RAM, ~40 MB peak for 1080p)
- [x] Automatic prefetch of adjacent pages
- [ ] Search hit overlay (red bbox highlight) *(planned)*
- [ ] Bookmarks / table of contents *(planned v2)*
- [ ] Annotation *(planned v3)*

## Prerequisites

### Windows
- Visual Studio 2022 (Desktop C++ workload)
- CMake ≥ 3.23 ([cmake.org](https://cmake.org/download/))
- Git for Windows

### Linux / T507
```bash
# Ubuntu / Debian dev machine
sudo apt install cmake ninja-build libsdl2-dev \
     gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu

# Or source your Timesys SDK environment-setup script
source /opt/timesys/<board>/environment-setup-*
```

## Build

### 1. Clone with submodules
```bash
git clone --recurse-submodules https://github.com/sliang951753/lvgl-pdf-viewer.git
cd lvgl-pdf-viewer
```

### 2. Windows (MSVC)
```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc
# Binary: build\windows-msvc\Debug\lvgl_pdf_viewer.exe
```

### 3. Linux desktop (quick dev iteration)
```bash
cmake --preset linux-host
cmake --build --preset linux-host
./build/linux-host/lvgl_pdf_viewer path/to/document.pdf
```

### 3.1 Termux + Termux:X11 local GUI simulation (Android)
```bash
# Install once
pkg install x11-repo termux-x11-nightly

# Launch Android-side Termux:X11 app
am start -n com.termux.x11/com.termux.x11.MainActivity

# Start X server bridge (in Termux)
termux-x11 :0 &

# Run viewer
export DISPLAY=:0
export XDG_RUNTIME_DIR=$PREFIX/tmp
./scripts/run_gui_termux_x11.sh third_party/mupdf/thirdparty/zlib/zlib.3.pdf 1280 720
```

### 3.2 Continuous-scroll regression smoke test
```bash
# Requires Termux:X11 display endpoint running (DISPLAY=:0)
./scripts/test_continuous_scroll_regression.sh
```
What it checks:
- multi-page PDF opens and renders page 0 + adjacent page 1 (two-page window baseline)
- no render-failed log line
- one-page PDF does not incorrectly render page 1

Design details: see `docs/continuous-scroll-design.md`.

### 3.3 Continuous-scroll replay regression (semi-automated)
```bash
# optional but recommended: xdotool (for wheel replay)
pkg install xdotool

./scripts/replay_continuous_scroll_regression.sh
```
What it does:
- launches viewer on multi-page PDF
- replays repeated wheel-down events (if `xdotool` available)
- analyzes log with `scripts/analyze_scroll_log.py`
- fails if multi-page doc never goes beyond page 1

### 3.4 Search regression (headless)
```bash
./scripts/test_search_regression.sh            # build + run
./scripts/test_search_regression.sh --no-build # just run
```
Headless smoke for `pdf_view_search()` via `tests/test_search_cli.c`. Cases:
- common word found from page 0 (with min-hits + first-page check)
- same query from a deeper start page still returns hits
- absent token returns zero hits (rc=1)
- truncation flag set when `max_hits` < total matches
- second document (`zlib.3.pdf`) to guard against doc-specific hardcoding

No display server required — safe in CI / on a vanilla shell.

### 4. T507 cross-compile
```bash
# Source Timesys SDK first:
source /opt/timesys/t507/environment-setup-aarch64-timesys-linux

cmake --preset t507-arm64
cmake --build --preset t507-arm64
# Transfer binary to board and run:
scp build/t507-arm64/lvgl_pdf_viewer root@<board-ip>:/usr/bin/
ssh root@<board-ip> lvgl_pdf_viewer /home/root/document.pdf 1024 600
```

## Usage
```
lvgl_pdf_viewer <path.pdf> [width] [height]

  path.pdf   — path to the PDF file to open
  width      — display width  in pixels (default: 800)
  height     — display height in pixels (default: 480)
```

## Project structure
```
lvgl-pdf-viewer/
├── CMakeLists.txt          Top-level build
├── CMakePresets.json       windows-msvc / t507-arm64 / linux-host
├── cmake/
│   ├── BuildMuPDF.cmake    MuPDF build via ExternalProject
│   ├── FindMuPDF.cmake     MuPDF library finder
│   └── toolchain-t507.cmake  aarch64 cross-compile toolchain
├── docs/
│   └── termux-x11-touch-debug.md   Touch/scroll fix design notes
├── scripts/
│   └── run_gui_termux_x11.sh       One-command local GUI run on Termux:X11
├── src/
│   ├── lv_conf.h           LVGL configuration
│   ├── main.c              Entry point (WinMain / main)
│   ├── pdf/
│   │   ├── pdf_view.h/c    MuPDF rendering + LRU cache
│   ├── ui/
│   │   ├── ui_main.h/c     LVGL screen, nav bar, gesture handling
│   └── platform/
│       ├── platform.h      Cross-platform HAL interface
│       ├── platform_win32.c  LVGL Win32 driver init
│       └── platform_sdl2.c   LVGL SDL2 driver init
├── third_party/
│   ├── lvgl/               git submodule — LVGL v9.5
│   └── mupdf/              git submodule — MuPDF 1.24.x
└── assets/
    └── sample.pdf
```

## Changelog

See [CHANGELOG.md](./CHANGELOG.md) for release-by-release changes.

## License

- Application code: **MIT**  
- LVGL: [MIT](https://github.com/lvgl/lvgl/blob/master/LICENCE.txt)  
- MuPDF: [AGPL-3.0](https://www.mupdf.com/licensing/) (free for open-source use; commercial licence available from Artifex)
