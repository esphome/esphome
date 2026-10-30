"""Summarize an interval-common.yaml capture: per interval the run count, first run after boot and the gaps.

usage: ivsum.py CAPTURE [CAPTURE ...]

The first run of an interval without a startup delay falls at a different time on every boot (the scheduler spreads
first runs), so compare the delayed intervals' first runs against their delay and the gaps, not the plain offsets.
"""

import argparse
import pathlib
import re
import sys

parser = argparse.ArgumentParser()
parser.add_argument("captures", nargs="+")
for path in parser.parse_args().captures:
    text = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    events = [
        (m.group(1), int(m.group(2)))
        for m in re.finditer(r"(BOOT|FIRE \w+|CTL .+?) (\d+)\s*$", text, re.MULTILINE)
    ]
    boot = next((t for k, t in events if k == "BOOT"), None)
    if boot is None:
        sys.exit(f"FAIL: {path} has no BOOT line")
    print(f"== {path}: boot at {boot} ms")
    for key in ("FIRE plain1s", "FIRE plain2500", "FIRE delay5s", "FIRE delay500"):
        times = [t for k, t in events if k == key]
        gaps = sorted({b - a for a, b in zip(times, times[1:], strict=False)})
        first = times[0] - boot if times else None
        print(
            f"  {key}: n={len(times)} first={first} ms after boot, gaps {gaps[:3]}..{gaps[-3:]}"
        )
    ctl = [(k, t) for k, t in events if k.startswith("CTL")]
    for k, t in ctl:
        print(f"  {k.strip()} at {t - boot} ms")
    if len(ctl) < 2:
        sys.exit(
            f"FAIL: {path} has {len(ctl)} of the 2 CTL lines (interval change and stop)"
        )
