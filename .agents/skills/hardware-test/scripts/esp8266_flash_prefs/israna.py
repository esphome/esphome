"""Summarise an isr-load capture: boots, crash lines, preference saves and flash errors, the pulse counter rate
(2 kHz expected) per one second window, binary sensor edges per second (10 expected), sync() times and the heap.

usage: israna.py CAPTURE
"""

import pathlib
import re
import statistics
import sys

CRASH_RE = re.compile(r"Exception \(|rst cause:[234]|Soft WDT|wdt reset|CRASH DETECTED")
PC_RE = re.compile(r"\]\[pc:\d+\]: d=(\d+) dt=(\d+)")
STAT_RE = re.compile(r"stat:\d+\]: edges=(\d+) heap=(\d+) maxblock=(\d+) up=(\d+)")
SYNC_RE = re.compile(r"sync us=(\d+)")

rates, edges, syncs, heaps, exc = [], [], [], [], []
lines = boots = saves = errs = pulses = span = 0
prev = None
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as fh:
    for ln in fh:
        lines += 1
        boots += "Log initialized" in ln
        saves += "preferences:" in ln and "Saving" in ln
        errs += "Erasing failed" in ln or "Writing failed" in ln
        if CRASH_RE.search(ln):
            exc.append(ln.rstrip())
        if m := PC_RE.search(ln):
            d, dt = int(m.group(1)), int(m.group(2))
            pulses += d
            span += dt
            rates.append(d * 1000 / dt)
        if m := STAT_RE.search(ln):
            e, up = int(m.group(1)), int(m.group(4))
            heaps.append(int(m.group(2)))
            if prev and up > prev[1]:
                edges.append((e - prev[0]) / (up - prev[1]))
            prev = (e, up)
        if m := SYNC_RE.search(ln):
            syncs.append(int(m.group(1)))
print(
    f"lines={lines} boots={boots} crash_lines={len(exc)} saves={saves} flash_errs={errs}"
)
if rates:
    print(
        f"pc samples={len(rates)} total_pulses={pulses} overall_hz={pulses * 1000 / span:.2f}"
    )
    print(
        f"rate min={min(rates):.1f} max={max(rates):.1f} median={statistics.median(rates):.1f}"
        f" out_of_1%={sum(abs(r - 2000) > 20 for r in rates)}"
    )
if edges:
    print(
        f"edges/s min={min(edges):.2f} max={max(edges):.2f} mean={statistics.mean(edges):.2f}"
    )
if syncs:
    print(
        f"sync us n={len(syncs)} median={statistics.median(syncs)} min={min(syncs)} max={max(syncs)}"
    )
if heaps:
    print(f"heap first={heaps[0]} min={min(heaps)} last={heaps[-1]}")
for ln in exc[:10]:
    print("  ", ln[:150])
