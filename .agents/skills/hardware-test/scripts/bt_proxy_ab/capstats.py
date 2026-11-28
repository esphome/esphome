"""Shared helpers for report.py and stparse.py: capture timestamps, median and p90, heap and warning summaries."""

from collections import Counter
import re
import statistics

WARN = re.compile(r"\[([WE])\]\[([^\]:]+)[^\]]*\](?:\[[A-Z_]+\])?: (.*)")
IDF_WARN = re.compile(
    r"esp-idf[^\]]*\](?:\[[A-Za-z_0-9]+\])?: ([WE]) \(\d+\) ([A-Za-z_0-9]+): (.*)"
)


def sec(hms: str) -> float:
    h, m, s = hms.split(":")
    return int(h) * 3600 + int(m) * 60 + float(s)


def unwrap(ts: list[float], ref: float | None = None) -> list[float]:
    """Time of day stamps onto one timeline across midnight; ref is a time the first stamp cannot be 12 h before."""
    out, day, prev = [], 0.0, ref
    for t in ts:
        if prev is not None and t + day < prev - 43200:
            day += 86400
        out.append(t + day)
        prev = out[-1]
    return out


def med(v, nd: int | None = 1):
    v = [x for x in v if x is not None]
    return round(statistics.median(v), nd) if v else None


def p90(v, nd: int | None = 1):
    v = sorted(x for x in v if x is not None)
    return round(v[min(len(v) - 1, int(0.9 * len(v)))], nd) if v else None


def print_heap(lines: list[tuple[float, str]], nd: int | None = 1) -> bool:
    """Summary of the heaplog.hpp HEAP lines; min_ever also takes the 100 ms HEAPMIN readings. False: none."""
    heap = [
        tuple(
            int(x)
            for x in re.search(r"free=(\d+) min=(\d+) largest=(\d+)", ln).groups()
        )
        for _, ln in lines
        if "HEAP free=" in ln
    ]
    lows = [int(m[1]) for _, ln in lines if (m := re.search(r"HEAPMIN min=(\d+)", ln))]
    if heap:
        print(
            "heap free min/med/last",
            min(h[0] for h in heap),
            med([h[0] for h in heap], nd),
            heap[-1][0],
            "min_ever",
            min([h[1] for h in heap] + lows),
            "largest_min",
            min(h[2] for h in heap),
        )
    else:
        print("heap: no HEAP lines (heaplog.hpp missing from the build?)")
    return bool(heap)


def print_warnings(lines: list[tuple[float, str]], top: int, mask: str) -> None:
    """The TOP most frequent warning and error lines, with MASK matches replaced by #."""
    wl = Counter()
    for _, ln in lines:
        if m := WARN.search(ln) or IDF_WARN.search(ln):
            wl[f"{m[1]} {m[2]}: " + re.sub(mask, "#", m[3])[:90]] += 1
    for k, v in wl.most_common(top):
        print(f"  {v:5d} {k}")
