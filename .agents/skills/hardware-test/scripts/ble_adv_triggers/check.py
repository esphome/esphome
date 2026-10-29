"""check.py CAPTURE [--zero-mac any|filter]: judge the last count report of a receiver capture.

Expected per trigger (see triggers.yaml): T1 AA01+AA05, T2 AA01, T3 nothing, T4 AA02, T5 AA03, T6 BB01, T7 BB04,
T8 the same count as T2, T9 (mac 00:00:00:00:00:00) AA01+AA05 with --zero-mac any or nothing with filter,
T10 AA03, T11 nothing. "other" on a trigger without a MAC filter is another device in range and is only reported.
Prints PASS or FAIL with the reason and exits 0 or 1.
"""

import argparse
import pathlib
import re
import sys

NO_MAC = {1, 7, 10}


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("capture")
    parser.add_argument("--zero-mac", choices=["any", "filter"], default="filter")
    args = parser.parse_args()
    zero = args.zero_mac
    text = pathlib.Path(args.capture).read_text(encoding="utf-8", errors="replace")
    rows: dict[int, dict[str, int]] = {}
    for m in re.finditer(r"count T(\d+):([^\n]*)", text):
        counts = dict(re.findall(r"(AA\d\d|BB\d\d|other)=(\d+)", m.group(2)))
        rows[int(m.group(1))] = {k: int(v) for k, v in counts.items()}
    if len(rows) != 11:
        print(
            f"FAIL: found {len(rows)} trigger rows, want 11 (no report in the capture?)"
        )
        return 1
    want = {
        1: {"AA01", "AA05"},
        2: {"AA01"},
        3: set(),
        4: {"AA02"},
        5: {"AA03"},
        6: {"BB01"},
        7: {"BB04"},
        8: {"AA01"},
        9: {"AA01", "AA05"} if zero == "any" else set(),
        10: {"AA03"},
        11: set(),
    }
    bad = []
    for t, keys in want.items():
        got = {k for k in rows[t] if k != "other"}
        if got != keys:
            bad.append(f"T{t} fired on {sorted(got)}, want {sorted(keys)}")
        if rows[t].get("other") and t not in NO_MAC:
            bad.append(
                f"T{t} has other={rows[t]['other']} although it filters on a MAC"
            )
        if keys and min(rows[t].get(k, 0) for k in keys) < 10:
            bad.append(
                f"T{t} has fewer than 10 hits on one of {sorted(keys)}: {rows[t]}"
            )
    if rows[8] != rows[2]:
        bad.append(f"T8 {rows[8]} differs from T2 {rows[2]} (same filter)")
    for t in sorted(rows):
        print(f"T{t}: {rows[t]}")
    print("FAIL: " + "; ".join(bad) if bad else "PASS")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
