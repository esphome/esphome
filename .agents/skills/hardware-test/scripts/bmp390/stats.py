"""Pressure statistics from a bmp390 capture.

usage: stats.py CAPTURE [SKIP_S]
Prints the read count and cadence, the noise (standard deviation, of consecutive differences and detrended over a
21 sample window), identical consecutive readings, the INT data ready rate per 10 s window and counts of errors,
warnings and harness lines. SKIP_S drops the readings before that many seconds after boot (IIR settling).
"""

import pathlib
import re
import statistics
import sys


def main() -> None:
    lines = pathlib.Path(sys.argv[1]).read_bytes().decode(errors="replace").splitlines()
    skip = float(sys.argv[2]) if len(sys.argv) > 2 else 0
    reads = [
        (int(m[1]), float(m[2]))
        for line in lines
        if (m := re.search(r"READ (\d+) ([\d.]+)", line))
    ]
    r = [p for t, p in reads if t / 1000 >= skip]
    if len(r) > 2:
        d = [b - a for a, b in zip(r, r[1:], strict=False)]
        same = sum(1 for x in d if x == 0)
        print(
            f"reads={len(reads)} used={len(r)} mean={statistics.mean(r):.4f} sd={statistics.pstdev(r) * 100:.2f}Pa "
            f"sd_diff={statistics.pstdev(d) * 100:.2f}Pa identical_consecutive={same} min={min(r):.4f} max={max(r):.4f}"
        )
    ts = [t for t, _ in reads]
    if len(ts) > 2:
        gaps = [b - a for a, b in zip(ts, ts[1:], strict=False)]
        print(f"cadence mean={statistics.mean(gaps):.0f}ms max_gap={max(gaps)}ms")
    ints = [
        (int(m[1]), int(m[2]))
        for line in lines
        if (m := re.search(r"INT edges=(\d+) in (\d+) ms", line))
    ]
    # the first window starts at boot, before data ready is on
    if rates := [e * 1000 / ms for e, ms in ints[1:] if ms > 9000]:
        print(f"INT rate Hz: {' '.join(f'{x:.2f}' for x in rates)}")
    for pat in (
        r"\[E\]",
        r"\[W\]",
        "XFERFAIL",
        "SOFTFAIL",
        "FAULT",
        "PROBE",
        "marked FAILED",
        "REGS",
    ):
        print(f"{pat}: {sum(1 for line in lines if re.search(pat, line))}")
    if len(r) > 30:
        k = 10
        res = [
            r[i] - statistics.mean(r[i - k : i + k + 1]) for i in range(k, len(r) - k)
        ]
        print(f"detrended sd (21 sample window)={statistics.pstdev(res) * 100:.3f}Pa")


if __name__ == "__main__":
    main()
