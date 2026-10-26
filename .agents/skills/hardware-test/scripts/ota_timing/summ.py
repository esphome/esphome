"""Median upload time per board and mode from otatime.py RESULT lines.
Usage: summ.py RESULTS_FILE [ROUNDS]
Exits 1 when any upload failed, an upload time could not be read, or (with ROUNDS) a board and mode has fewer rows."""

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
rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 0
failed = unparsed = short = 0
for (board, mode), v in sorted(by.items()):
    ok = sorted(r["up"] for r in v if r["rc"] == 0 and r["up"])
    fails = sum(r["rc"] != 0 for r in v)
    failed += fails
    unparsed += sum(r["rc"] == 0 and r["up"] is None for r in v)
    short += len(v) < rounds
    udp = sum(r["udp"] for r in v if r["rc"] == 0)
    med = statistics.median(ok) if ok else None
    print(
        f"{board:24} {mode:12} n={len(v)} fail={fails} udp={udp} median={med} upload s: {ok}"
    )
if not rows:
    print("FAIL: no results")
elif failed or unparsed or short:
    print(
        f"FAIL: {failed} failed uploads, {unparsed} without an upload time, {short} groups under {rounds} rows"
    )
else:
    print("PASS")
sys.exit(0 if rows and not (failed or unparsed or short) else 1)
