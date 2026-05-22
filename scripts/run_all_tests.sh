#!/usr/bin/env bash
# =============================================================================
# scripts/run_all_tests.sh
#
# Builds the linux-host preset once, then runs every regression script
# that can execute in the current environment.
#
# Headless tests always run:
#   - test_search_regression.sh
#   - test_pageapi_regression.sh
#
# GUI tests run only when DISPLAY is set (Termux:X11 / X server reachable):
#   - test_continuous_scroll_regression.sh
#   - replay_continuous_scroll_regression.sh  (also needs xdotool)
#
# Exit code: number of failing suites (0 = all pass).
# =============================================================================
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "[run_all_tests] configuring + building linux-host..."
cmake --preset linux-host >/dev/null || { echo "CMAKE CONFIG FAIL" >&2; exit 1; }
cmake --build build/linux-host -j4 >/dev/null \
    || { echo "BUILD FAIL" >&2; exit 1; }

FAIL=0
SUMMARY=()

run_suite() {
    local label="$1" cmd="$2"
    echo
    echo "============================================================"
    echo " ▶  $label"
    echo "============================================================"
    if bash -c "$cmd"; then
        SUMMARY+=("PASS  $label")
    else
        SUMMARY+=("FAIL  $label")
        FAIL=$((FAIL + 1))
    fi
}

# Headless — always.
run_suite "search regression (headless)" \
    "scripts/test_search_regression.sh --no-build"

run_suite "pdf_view API regression (headless)" \
    "scripts/test_pageapi_regression.sh --no-build"

# GUI — only when a display server is available.
if [[ -n "${DISPLAY:-}" ]]; then
    run_suite "continuous-scroll smoke (GUI)" \
        "scripts/test_continuous_scroll_regression.sh"

    if command -v xdotool >/dev/null 2>&1; then
        run_suite "continuous-scroll replay (GUI + xdotool)" \
            "scripts/replay_continuous_scroll_regression.sh"
    else
        SUMMARY+=("SKIP  continuous-scroll replay (xdotool missing)")
    fi
else
    SUMMARY+=("SKIP  GUI suites (DISPLAY unset)")
fi

echo
echo "============================================================"
echo " SUMMARY"
echo "============================================================"
for line in "${SUMMARY[@]}"; do echo "  $line"; done
echo "============================================================"
exit "$FAIL"
