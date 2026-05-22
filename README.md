# lvgl-pdf-viewer

A cross-platform PDF viewer built with **LVGL 9.5** and **MuPDF**, targeting:

| Platform | Display driver | Compiler |
|---|---|---|
| Windows | Win32 API (built-in LVGL driver) | MSVC (VS 2022) |
| Allwinner T507 / Timesys Linux | SDL2 | aarch64-linux-gnu-gcc |
| Linux desktop (dev) | SDL2 | GCC / Clang |
| Termux / Android (dev sim) | SDL2 via Termux:X11 | clang (NDK toolchain in Termux) |

Render pipeline: `MuPDF → ARGB8888 pixmap → lv_image_dsc_t → LVGL SW renderer`

## Features

- [x] Open and display local PDF files
- [x] Fit-to-width page rendering
- [x] **Virtual full-document scroll** — content size equals total doc
      height; native LVGL scrollbar reflects whole-document position;
      scroll-back to earlier pages works without gesture plumbing.
- [x] Word-like continuous page flow (no hard page flips).
- [x] Button navigation (Prev/Next) — disables correctly at document
      edges; re-enables reliably (LVGL-9 disabled-state bug fixed).
- [x] Page-jump dialog (clickable page indicator → numeric pad with
      range validation).
- [x] Zoom in/out (25% steps, 50%–400%) — pages re-center and
      horizontal pan resets after each zoom.
- [x] **Text search** (full-document or single-page) with hit
      navigation (Prev/Next/Clear/Close).
- [x] **Search hit overlay**: red bbox rectangles drawn on visible
      pages; active hit gets thicker border + translucent fill.
      Overlay rectangles share the virtual-scroll coordinate system
      (parented to `g_scroll_area`).
- [x] LRU page cache (5 pages in RAM, ~40 MB peak for 1080p).
- [x] Automatic prefetch of adjacent pages.
- [ ] Bookmarks / table of contents *(planned v2)*
- [ ] Annotation *(planned v3)*

## Documentation

| Doc | Contents |
|---|---|
| [docs/architecture.md](./docs/architecture.md) | Layered module map, scroll/search/render pipelines, LVGL-9 pitfalls, testing strategy. **Read first.** |
| [docs/virtual-scroll-design.md](./docs/virtual-scroll-design.md) | Virtual full-document scroll model, zoom recentering, search-jump math. |
| [docs/continuous-scroll-design.md](./docs/continuous-scroll-design.md) | Word-like page flow rationale (historical 2-page-window approach kept for context). |
| [docs/termux-x11-touch-debug.md](./docs/termux-x11-touch-debug.md) | Touch / SDL2 / Termux:X11 input fixes and root-cause analysis. |
| [CHANGELOG.md](./CHANGELOG.md) | Release-by-release changes. |

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
# Termux:X11 client APK: https://github.com/termux/termux-x11/releases
#   → install app-arm64-v8a-debug.apk

# Start X server bridge (in Termux)
termux-x11 :0 &

# Launch Android-side Termux:X11 app (now that the package is installed)
am start -n com.termux.x11/com.termux.x11.MainActivity

# Run viewer
export DISPLAY=:0
export XDG_RUNTIME_DIR=$PREFIX/tmp
./scripts/run_gui_termux_x11.sh third_party/mupdf/thirdparty/zlib/zlib.3.pdf 1280 720
```

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

## Testing

Run **everything** that the environment supports with one command:

```bash
./scripts/run_all_tests.sh
```

The aggregator builds the `linux-host` preset once, then runs:

| Suite | Type | Requires |
|---|---|---|
| `test_search_regression.sh` | headless | linux-host build |
| `test_pageapi_regression.sh` | headless | linux-host build |
| `test_continuous_scroll_regression.sh` | GUI smoke | `DISPLAY` reachable |
| `replay_continuous_scroll_regression.sh` | GUI replay | `DISPLAY` + `xdotool` |

GUI suites are skipped automatically when `DISPLAY` is unset, so the
script is safe on a vanilla CI shell.

### Individual suites

```bash
# 1) Headless: pdf_view text search (5 cases, 2 docs)
./scripts/test_search_regression.sh              # build + run
./scripts/test_search_regression.sh --no-build   # just run

# 2) Headless: non-search pdf_view APIs (open/close, page_count,
#    page_size with out-of-range / negative index, render, prefetch,
#    cache_clear, re-render). 7 checks × 3 docs.
./scripts/test_pageapi_regression.sh

# 3) GUI smoke (needs Termux:X11 running, DISPLAY=:0)
./scripts/test_continuous_scroll_regression.sh

# 4) GUI replay with simulated wheel events (needs xdotool)
pkg install xdotool
./scripts/replay_continuous_scroll_regression.sh
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
│   ├── architecture.md             Module map + pipelines + pitfalls
│   ├── virtual-scroll-design.md    Virtual full-doc scroll model
│   ├── continuous-scroll-design.md Continuous page flow design
│   └── termux-x11-touch-debug.md   Touch/scroll fix design notes
├── scripts/
│   ├── run_all_tests.sh                       One command to run all
│   ├── run_gui_termux_x11.sh                  One-shot GUI launch
│   ├── test_search_regression.sh              Headless search suite
│   ├── test_pageapi_regression.sh             Headless pdf_view API
│   ├── test_continuous_scroll_regression.sh   GUI smoke
│   ├── replay_continuous_scroll_regression.sh GUI replay
│   └── analyze_scroll_log.py                  Log analyzer helper
├── src/
│   ├── lv_conf.h           LVGL configuration
│   ├── main.c              Entry point (WinMain / main)
│   ├── pdf/
│   │   └── pdf_view.h/c    MuPDF rendering + LRU cache + text search
│   ├── ui/
│   │   └── ui_main.h/c     LVGL screen, virtual scroll, dialogs,
│   │                       search overlay
│   └── platform/
│       ├── platform.h      Cross-platform HAL interface
│       ├── platform_win32.c  LVGL Win32 driver init
│       └── platform_sdl2.c   LVGL SDL2 driver init
├── tests/
│   ├── test_search_cli.c   pdf_view_search() driver
│   └── test_pageapi_cli.c  page_count/page_size/render/prefetch driver
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
