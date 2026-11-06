"""mx_an.py SERIAL: per MAX7219 chain, the transaction counts and how the latched digits evolve.

Reads the MxSniff report lines (mxsim.hpp). Exits 1 when there are none, a chain had malformed transactions, or a
chain's transaction count is 0 or never grew between its first and last report.
"""

import collections
import pathlib
import re
import sys


def main() -> int:
    seen: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    first: dict[str, int] = {}
    last: dict[str, tuple[int, int]] = {}
    for line in (
        pathlib.Path(sys.argv[1])
        .read_text(encoding="utf-8", errors="replace")
        .splitlines()
    ):
        if m := re.search(r"MX (\w+) tx=(\d+) bad=(\d+) .*digits=(.*)$", line):
            seen[m.group(1)][m.group(4).strip()] += 1
            first.setdefault(m.group(1), int(m.group(2)))
            last[m.group(1)] = (int(m.group(2)), int(m.group(3)))
    if not seen:
        print("FAIL: no MX report lines")
        return 1
    for k in sorted(seen):
        print(
            f"{k}: transactions={last[k][0]} malformed={last[k][1]} distinct digit sets={len(seen[k])}"
        )
        for d, n in seen[k].most_common(4):
            print(f"    {n:3d}x {d}")
    bad = [k for k, (_, b) in last.items() if b]
    if bad:
        print(f"FAIL: malformed transactions on {bad}")
        return 1
    reports = {k: sum(c.values()) for k, c in seen.items()}
    stuck = [
        k
        for k, (tx, _) in last.items()
        if not tx or (reports[k] > 1 and tx <= first[k])
    ]
    if stuck:
        print(f"FAIL: no new transactions on {stuck}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
