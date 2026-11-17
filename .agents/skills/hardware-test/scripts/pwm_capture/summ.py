"""Total the pwmcap WIN lines of one or more captures.

usage: summ.py CAPTURE...
Sums periods, PCNT edges, out-of-tolerance high times and periods, level changes, spikes and reversals over every
full 10 s window, and prints the high time and period ranges. Window boundaries follow RMT chunks, so sum over the
run instead of comparing single windows. Exits 1 when a capture has no full window, no periods, or RMT and PCNT
period totals more than 1% apart.
"""

import pathlib
import re
import sys

WIN = re.compile(
    r"WIN dt=(\d+)ms n=(\d+) pcnt=(-?\d+) chunks=\d+ zero=(\d+) idle=(\d+) "
    r"h\[min=(\d+) mode=(\d+) max=(\d+) out=(\d+) dev=(\d+)\] "
    r"p\[min=(\d+) mode=(\d+) max=(\d+) out=(\d+) dev=(\d+)\] chg=(\d+) spikes=(\d+) rev=(\d+)"
)


def summarize(path: str) -> bool:
    tot = dict.fromkeys(
        ("n", "pcnt", "h_out", "p_out", "spikes", "rev", "chg", "zero"), 0
    )
    hmin, hmax, pmin, pmax, hdev, pdev, wins, idle = 10**9, 0, 10**9, 0, 0, 0, 0, 0
    modes: set[int] = set()
    for line in pathlib.Path(path).read_bytes().decode(errors="replace").splitlines():
        m = WIN.search(line)
        if not m or int(m[1]) < 9000:
            continue
        v = [int(x) for x in m.groups()]
        wins += 1
        tot["n"] += v[1]
        tot["pcnt"] += v[2]
        tot["zero"] += v[3]
        idle = v[4]
        hmin, hmax, hdev = min(hmin, v[5]), max(hmax, v[7]), max(hdev, v[9])
        modes.add(v[6])
        tot["h_out"] += v[8]
        pmin, pmax, pdev = min(pmin, v[10]), max(pmax, v[12]), max(pdev, v[14])
        tot["p_out"] += v[13]
        tot["chg"] += v[15]
        tot["spikes"] += v[16]
        tot["rev"] += v[17]
    print(
        path,
        f"windows={wins} ({wins * 10}s)",
        tot,
        f"h {hmin}..{hmax} modes={sorted(modes)[:6]} hdev={hdev} p {pmin}..{pmax} pdev={pdev} idle={idle}",
    )
    if wins and tot["n"] and abs(tot["n"] - tot["pcnt"]) * 100 <= tot["n"]:
        return True
    print(f"FAIL: {path} windows={wins} n={tot['n']} pcnt={tot['pcnt']}")
    return False


def main() -> None:
    results = [summarize(path) for path in sys.argv[1:]]
    if not results or not all(results):
        sys.exit(1)


if __name__ == "__main__":
    main()
