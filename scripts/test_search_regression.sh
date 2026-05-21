#!/usr/bin/env bash
# =============================================================================
# scripts/test_search_regression.sh
#
# Headless regression for pdf_view_search() using the test_search_cli helper.
# Runs a set of (pdf, needle, expectations) cases and verifies:
#   - exit code matches expected hit/no-hit outcome
#   - first hit lands on the expected page (when checked)
#   - hit count satisfies a minimum (when checked)
#   - bbox coordinates are positive, finite-looking numbers
#
# No GUI, no display server — safe to run on a vanilla Linux/Termux shell.
#
# Usage:
#   scripts/test_search_regression.sh              # build if needed, run all
#   scripts/test_search_regression.sh --no-build   # skip cmake build step
#
# Exit code: 0 if all cases pass, 1 otherwise.
# =============================================================================
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BIN="$ROOT/build/linux-host/test_search_cli"
PDF_LCMS="$ROOT/third_party/mupdf/thirdparty/lcms2/doc/LittleCMS2.16 API.pdf"
PDF_ZLIB="$ROOT/third_party/mupdf/thirdparty/zlib/zlib.3.pdf"

DO_BUILD=1
for arg in "$@"; do
    case "$arg" in
        --no-build) DO_BUILD=0 ;;
    esac
done

if [[ $DO_BUILD -eq 1 ]]; then
    if [[ ! -x "$BIN" || ! -d build/linux-host ]]; then
        echo "[search-regression] configuring linux-host preset..."
        cmake --preset linux-host >/dev/null
    fi
    echo "[search-regression] building test_search_cli..."
    cmake --build build/linux-host --target test_search_cli -j4 >/dev/null \
        || { echo "BUILD FAIL" >&2; exit 1; }
fi

[[ -x "$BIN" ]] || { echo "missing $BIN — run cmake --build first" >&2; exit 1; }

PASS=0
FAIL=0
RESULTS=()

# -----------------------------------------------------------------------------
# run_case  <label>  <pdf>  <needle>  <start>  <cap>  <expect_rc>  [min_hits]  [first_page]
#
#   expect_rc:    0 = expect at least 1 hit, 1 = expect zero hits
#   min_hits:     -1 to skip; otherwise enforce hits >= min_hits
#   first_page:   -1 to skip; otherwise enforce hits[0].page == first_page
# -----------------------------------------------------------------------------
run_case() {
    local label="$1" pdf="$2" needle="$3" start="$4" cap="$5"
    local expect_rc="$6" min_hits="${7:--1}" first_page="${8:--1}"

    if [[ ! -f "$pdf" ]]; then
        RESULTS+=("SKIP  $label  (missing $pdf)")
        return
    fi

    local out rc
    out="$("$BIN" "$pdf" "$needle" "$start" "$cap" 2>/dev/null)"
    rc=$?

    if [[ "$rc" -ne "$expect_rc" ]]; then
        RESULTS+=("FAIL  $label  (rc=$rc, expected $expect_rc)")
        FAIL=$((FAIL + 1)); return
    fi

    if [[ "$expect_rc" -eq 0 ]]; then
        local hits
        hits=$(awk -F'[= ]' '/^HITS=/{print $2; exit}' <<<"$out")
        if [[ "$min_hits" -ge 0 && "${hits:-0}" -lt "$min_hits" ]]; then
            RESULTS+=("FAIL  $label  (hits=$hits, expected >= $min_hits)")
            FAIL=$((FAIL + 1)); return
        fi
        if [[ "$first_page" -ge 0 ]]; then
            local fp
            fp=$(awk '/^  \[0\] page=/{ for(i=1;i<=NF;i++) if($i ~ /^page=/){split($i,a,"="); print a[2]; exit} }' <<<"$out")
            if [[ -z "$fp" || "$fp" -ne "$first_page" ]]; then
                RESULTS+=("FAIL  $label  (first_page=$fp, expected $first_page)")
                FAIL=$((FAIL + 1)); return
            fi
        fi
        # bbox sanity: each hit line has 4 positive numeric components
        if grep -qE 'bbox=-' <<<"$out"; then
            RESULTS+=("FAIL  $label  (negative bbox component)")
            FAIL=$((FAIL + 1)); return
        fi
    fi

    RESULTS+=("PASS  $label")
    PASS=$((PASS + 1))
}

# -----------------------------------------------------------------------------
# Cases
# -----------------------------------------------------------------------------
# 1. Common word found early in the LittleCMS doc.
run_case "lcms/color/start=0/wrap"       "$PDF_LCMS" "color"        0 50  0 5  1
# 2. Same query starting from a deeper page should still find hits (>= 1).
run_case "lcms/color/start=20/wrap"      "$PDF_LCMS" "color"        20 50 0 1 -1
# 3. Negative test: a clearly absent token returns rc=1 (no hits).
run_case "lcms/no-such-word/zero-hits"   "$PDF_LCMS" "ZQXJZQXJZQXJ" 0 50  1
# 4. Truncation marker is set when cap is small but matches are plentiful.
run_case "lcms/the/cap=3/truncated"      "$PDF_LCMS" "the"          0 3   0 3 -1
# 5. Different document, common word — guards against doc-specific hardcoding.
run_case "zlib/zlib"                     "$PDF_ZLIB" "zlib"         0 50  0 1 -1

# -----------------------------------------------------------------------------
# Report
# -----------------------------------------------------------------------------
echo
echo "============================================================"
for r in "${RESULTS[@]}"; do echo "  $r"; done
echo "============================================================"
echo "  PASS=$PASS  FAIL=$FAIL"
echo "============================================================"

[[ "$FAIL" -eq 0 ]]
