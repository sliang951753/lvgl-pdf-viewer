#!/usr/bin/env bash
# =============================================================================
# scripts/test_pageapi_regression.sh
#
# Headless regression for the non-search pdf_view APIs (open/close,
# page_count, page_size, render_page, prefetch, cache_clear) via the
# tests/test_pageapi_cli helper.
#
# No GUI, no display server — safe in CI.
#
# Usage:
#   scripts/test_pageapi_regression.sh              # build then run
#   scripts/test_pageapi_regression.sh --no-build   # skip cmake build
# =============================================================================
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BIN="$ROOT/build/linux-host/test_pageapi_cli"
PDF_LCMS="$ROOT/third_party/mupdf/thirdparty/lcms2/doc/LittleCMS2.16 API.pdf"
PDF_ZLIB="$ROOT/third_party/mupdf/thirdparty/zlib/zlib.3.pdf"
PDF_ONE="$ROOT/third_party/mupdf/thirdparty/extract/test/table.pdf"

DO_BUILD=1
for arg in "$@"; do
    case "$arg" in
        --no-build) DO_BUILD=0 ;;
    esac
done

if [[ $DO_BUILD -eq 1 ]]; then
    if [[ ! -d build/linux-host ]]; then
        echo "[pageapi-regression] configuring linux-host preset..."
        cmake --preset linux-host >/dev/null
    fi
    echo "[pageapi-regression] building test_pageapi_cli..."
    cmake --build build/linux-host --target test_pageapi_cli -j4 >/dev/null \
        || { echo "BUILD FAIL" >&2; exit 1; }
fi

[[ -x "$BIN" ]] || { echo "missing $BIN — run cmake --build first" >&2; exit 1; }

FAIL=0
run() {
    local label="$1" pdf="$2"
    if [[ ! -f "$pdf" ]]; then
        echo "SKIP  $label  (missing $pdf)"; return
    fi
    echo "----- $label : $pdf"
    if ! "$BIN" "$pdf"; then
        FAIL=$((FAIL + 1))
        echo "FAIL  $label"
    fi
}

run "multi-page (lcms2 API)" "$PDF_LCMS"
run "small doc (zlib.3)"     "$PDF_ZLIB"
run "single-page (table)"    "$PDF_ONE"

echo "============================================================"
echo "  FAIL=$FAIL"
echo "============================================================"
[[ "$FAIL" -eq 0 ]]
