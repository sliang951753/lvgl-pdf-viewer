#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/linux-host/lvgl_pdf_viewer"
PDF_DEFAULT="third_party/mupdf/thirdparty/lcms2/doc/LittleCMS2.16 API.pdf"
W=1280
H=720

PDF="${1:-$PDF_DEFAULT}"
OUT_DIR="$ROOT/logs"
OUT="$OUT_DIR/replay-continuous-scroll.log"
mkdir -p "$OUT_DIR"

if [[ ! -x "$BIN" ]]; then
  echo "[FAIL] binary not found: $BIN"
  echo "       build first: cmake --preset linux-host && cmake --build --preset linux-host -j"
  exit 1
fi

cd "$ROOT"

echo "[STEP] start viewer"
: > "$OUT"

env DISPLAY=:0 XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-$PREFIX/tmp}" \
  "$BIN" "$PDF" "$W" "$H" > "$OUT" 2>&1 &
PID=$!

cleanup() {
  kill "$PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

sleep 1.2

echo "[STEP] replay wheel scroll (xdotool if available)"
if command -v xdotool >/dev/null 2>&1; then
  # Focus window and send repeated wheel-down to mimic reader scroll.
  xdotool search --name "LVGL PDF Viewer" windowactivate --sync %@ || true
  for _ in $(seq 1 120); do
    xdotool click 5 || true  # wheel down
    sleep 0.02
  done
else
  echo "[WARN] xdotool not found, fallback to timed run only"
  sleep 4
fi

sleep 1
kill "$PID" >/dev/null 2>&1 || true
wait "$PID" 2>/dev/null || true

echo "[STEP] analyze log"
python3 "$ROOT/scripts/analyze_scroll_log.py" "$OUT"

echo "[PASS] replay regression completed"
