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
failed = False
for path in parser.parse_args().captures:
    text = pathlib.Path(path).read_text(encoding="utf-8", errors="replace")
    events = [
        (m.group(1), int(m.group(2)))
        for m in re.finditer(r"(BOOT|FIRE \w+|CTL .+?) (\d+)\s*$", text, re.MULTILINE)
    ]
    boot = next((t for k, t in events if k == "BOOT"), None)
    if boot is None:
        print(f"FAIL: {path} has no BOOT line")
        failed = True
        continue
    print(f"== {path}: boot at {boot} ms")
    fails = []
    fires = {}
    for key in ("FIRE plain1s", "FIRE plain2500", "FIRE delay5s", "FIRE delay500"):
        times = [t for k, t in events if k == key]
        fires[key] = times
        gaps = sorted({b - a for a, b in zip(times, times[1:], strict=False)})
        first = times[0] - boot if times else None
        print(
            f"  {key}: n={len(times)} first={first} ms after boot, gaps {gaps[:3]}..{gaps[-3:]}"
        )
    ctl = [(k, t) for k, t in events if k.startswith("CTL")]
    for k, t in ctl:
        print(f"  {k.strip()} at {t - boot} ms")
    if len(ctl) < 2:
        fails.append(f"{len(ctl)} of the 2 CTL lines (interval change and stop)")
    # Run counts over a 40 s capture, the delays, the steady periods
    for key, minimum in (
        ("FIRE plain1s", 5),
        ("FIRE plain2500", 8),
        ("FIRE delay5s", 3),
        ("FIRE delay500", 8),
    ):
        if len(fires[key]) < minimum:
            fails.append(
                f"{key} ran {len(fires[key])} times, expected {minimum} or more"
            )
    for key, delay in (("FIRE delay5s", 5000), ("FIRE delay500", 500)):
        if fires[key] and fires[key][0] - boot < delay:
            fails.append(
                f"{key} first ran {fires[key][0] - boot} ms after boot, before its delay"
            )
    for key, period in (("FIRE plain2500", 2500), ("FIRE delay500", 3000)):
        times = fires[key]
        if any(
            abs(b - a - period) > 50 for a, b in zip(times, times[1:], strict=False)
        ):
            fails.append(f"{key} has a gap more than 50 ms off {period} ms")
    if len(ctl) >= 2:
        changed, stopped = ctl[0][1], ctl[1][1]
        after = [t for t in fires["FIRE plain1s"] if changed < t <= stopped]
        if len(after) < 2 or any(
            abs(b - a - 500) > 50 for a, b in zip(after, after[1:], strict=False)
        ):
            fails.append("plain1s did not run every 500 ms after the change")
        for key in ("FIRE plain1s", "FIRE delay5s"):
            late = [t for t in fires[key] if t > stopped + 20]
            if late:
                fails.append(f"{key} ran {len(late)} times after stop_poller()")
    if fails:
        print("FAIL: " + "; ".join(fails))
        failed = True
    else:
        print("PASS")
if failed:
    sys.exit(1)
