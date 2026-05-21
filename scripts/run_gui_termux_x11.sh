#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

# Usage:
#   ./scripts/run_gui_termux_x11.sh <pdf_path> [width] [height]
# Example:
#   ./scripts/run_gui_termux_x11.sh third_party/mupdf/thirdparty/zlib/zlib.3.pdf 1280 720

PDF_PATH="${1:-}"
W="${2:-1280}"
H="${3:-720}"

if [[ -z "$PDF_PATH" ]]; then
  echo "Usage: $0 <path.pdf> [width] [height]"
  exit 1
fi

if [[ ! -f "$PDF_PATH" ]]; then
  echo "PDF not found: $PDF_PATH"
  exit 1
fi

# Start X11 bridge in background (requires Termux:X11 Android app installed)
if ! pgrep -f "termux-x11 :0" >/dev/null 2>&1; then
  termux-x11 :0 >/dev/null 2>&1 &
  sleep 1
fi

export DISPLAY=:0
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-$PREFIX/tmp}"

exec ./build/linux-host/lvgl_pdf_viewer "$PDF_PATH" "$W" "$H"
