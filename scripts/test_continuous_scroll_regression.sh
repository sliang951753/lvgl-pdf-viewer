#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/linux-host/lvgl_pdf_viewer"
PDF_DEFAULT="third_party/mupdf/thirdparty/lcms2/doc/LittleCMS2.16 API.pdf"
W=1280
H=720

PDF="${1:-$PDF_DEFAULT}"
OUT_DIR="$ROOT/logs"
OUT="$OUT_DIR/regression-continuous-scroll.log"
mkdir -p "$OUT_DIR"

if [[ ! -x "$BIN" ]]; then
  echo "[FAIL] binary not found: $BIN"
  echo "       build first: cmake --preset linux-host && cmake --build --preset linux-host -j"
  exit 1
fi

cd "$ROOT"

echo "[INFO] running viewer smoke for: $PDF"
: > "$OUT"

timeout 8s env DISPLAY=:0 XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-$PREFIX/tmp}" \
  "$BIN" "$PDF" "$W" "$H" > "$OUT" 2>&1 || true

assert_contains() {
  local pattern="$1"
  local msg="$2"
  if ! grep -Eq "$pattern" "$OUT"; then
    echo "[FAIL] $msg"
    echo "[INFO] log: $OUT"
    exit 1
  fi
}

assert_not_contains() {
  local pattern="$1"
  local msg="$2"
  if grep -Eq "$pattern" "$OUT"; then
    echo "[FAIL] $msg"
    echo "[INFO] log: $OUT"
    exit 1
  fi
}

assert_contains "Opened '.*' \([0-9]+ pages\)" "pdf open/page-count line missing"
assert_contains "Rendered page 0" "page 0 not rendered"
assert_contains "Rendered page 1" "adjacent page (for two-page window) not rendered"
assert_not_contains "render failed" "render failure found"

# Optional sanity: one-page file should not render page 1
ONEPAGE="third_party/mupdf/thirdparty/extract/test/table.pdf"
: > "$OUT"
timeout 5s env DISPLAY=:0 XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-$PREFIX/tmp}" \
  "$BIN" "$ONEPAGE" "$W" "$H" > "$OUT" 2>&1 || true
assert_contains "Opened '.*' \(1 pages\)" "one-page pdf open line missing"
assert_not_contains "Rendered page 1" "one-page file should not render page 1"

echo "[PASS] continuous-scroll regression smoke passed"
echo "[INFO] log: $OUT"
