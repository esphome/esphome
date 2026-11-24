"""Summarise an isr-load capture: boots, crash lines, preference saves and flash errors, the pulse counter rate
(2 kHz expected) per one second window, binary sensor edges per second (10 expected), sync() times and the heap.

usage: israna.py CAPTURE
"""

import pathlib
import re
import statistics
import sys

with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as fh:
    L = fh.read().splitlines()
rates, ds, dts, edges, syncs = [], [], [], [], []
boots = sum("Log initialized" in ln for ln in L)
exc = [
    ln for ln in L if re.search(r"Exception \(|rst cause:[234]|Soft WDT|wdt reset", ln)
]
saves = sum("preferences:" in ln and "Saving" in ln for ln in L)
errs = sum(("Erasing failed" in ln or "Writing failed" in ln) for ln in L)
prev = None
heaps = []
for ln in L:
    if m := re.search(r"\]\[pc:\d+\]: d=(\d+) dt=(\d+)", ln):
        d, dt = int(m.group(1)), int(m.group(2))
        ds.append(d)
        dts.append(dt)
        rates.append(d * 1000 / dt)
    if m := re.search(
        r"stat:\d+\]: edges=(\d+) heap=(\d+) maxblock=(\d+) up=(\d+)", ln
    ):
        e, up = int(m.group(1)), int(m.group(4))
        heaps.append(int(m.group(2)))
        if prev and up > prev[1]:
            edges.append((e - prev[0]) / (up - prev[1]))
        prev = (e, up)
    if m := re.search(r"sync us=(\d+)", ln):
        syncs.append(int(m.group(1)))
print(
    f"lines={len(L)} boots={boots} crash_lines={len(exc)} saves={saves} flash_errs={errs}"
)
if rates:
    print(
        f"pc samples={len(rates)} total_pulses={sum(ds)} overall_hz={sum(ds) * 1000 / sum(dts):.2f}"
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
