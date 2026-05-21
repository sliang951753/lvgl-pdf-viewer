#!/usr/bin/env python3
import re
import sys
from pathlib import Path

if len(sys.argv) < 2:
    print("Usage: analyze_scroll_log.py <log_file>")
    sys.exit(2)

log_path = Path(sys.argv[1])
if not log_path.exists():
    print(f"[FAIL] log not found: {log_path}")
    sys.exit(2)

opened_pages = None
rendered = []
render_failed = False

re_open = re.compile(r"Opened '.*' \((\d+) pages\)")
re_render = re.compile(r"Rendered page (\d+)\s+\d+x\d+\s+zoom=([0-9.]+)")

for line in log_path.read_text(errors="ignore").splitlines():
    if "render failed" in line:
        render_failed = True
    m = re_open.search(line)
    if m:
        opened_pages = int(m.group(1))
    m = re_render.search(line)
    if m:
        rendered.append((int(m.group(1)), float(m.group(2))))

print(f"[INFO] log: {log_path}")
print(f"[INFO] opened_pages={opened_pages}")
print(f"[INFO] rendered_events={len(rendered)}")

if opened_pages is None:
    print("[FAIL] missing 'Opened ... (N pages)' line")
    sys.exit(1)

if render_failed:
    print("[FAIL] found 'render failed' in log")
    sys.exit(1)

if not rendered:
    print("[FAIL] no rendered-page events")
    sys.exit(1)

pages = [p for p, _ in rendered]
unique_pages = sorted(set(pages))
max_page = max(pages)

print(f"[INFO] unique_pages_count={len(unique_pages)} max_page={max_page}")
print(f"[INFO] first_12_rendered={pages[:12]}")

# Core stuck detection: if multi-page doc but never goes beyond page 1,
# continuous flow likely regressed.
if opened_pages >= 3 and max_page <= 1:
    print("[FAIL] multi-page file stayed at page<=1 (possible continuous-flow regression)")
    sys.exit(1)

# Soft warning for potential skip pattern in rendered set.
if unique_pages:
    missing = [i for i in range(0, max_page + 1) if i not in set(unique_pages)]
    if missing:
        print(f"[WARN] missing rendered pages within [0..{max_page}]: {missing[:20]}")

print("[PASS] scroll log analysis passed")
