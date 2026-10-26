"""Median upload time per board and mode from otatime.py RESULT lines.
Usage: summ.py RESULTS_FILE
Exits 1 when any upload failed."""

import json
from pathlib import Path
import re
import statistics
import sys

rows = [
    json.loads(line.split("RESULT ", 1)[1])
    for line in Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
    if "RESULT " in line
]
by: dict[tuple[str, str], list[dict]] = {}
for r in rows:
    m = re.search(r"upload ([\d.]+)", r["took"])
    r["up"] = float(m[1]) if m else None
    # TAG is PHASE-BOARD-ROUND; group per phase and board
    by.setdefault((r["board"].rsplit("-", 1)[0], r["mode"]), []).append(r)
failed = 0
for (board, mode), v in sorted(by.items()):
    ok = sorted(r["up"] for r in v if r["rc"] == 0 and r["up"])
    fails = sum(r["rc"] != 0 for r in v)
    failed += fails
    udp = sum(r["udp"] for r in v if r["rc"] == 0)
    med = statistics.median(ok) if ok else None
    print(
        f"{board:24} {mode:12} n={len(v)} fail={fails} udp={udp} median={med} upload s: {ok}"
    )
print(
    "PASS"
    if rows and not failed
    else f"FAIL: {failed} failed uploads"
    if rows
    else "FAIL: no results"
)
sys.exit(0 if rows and not failed else 1)
