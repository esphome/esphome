"""lcd_an.py SERIAL: rebuild each simulated LCD's rows from the lcdsim report lines and check them.

Expects three lcd_pcf8574 displays on LcdSimBus: 0x27 20x4 whose lambda writes row i as 16 x chr(97 + i) plus a 4
digit counter (or only an "x" in the last cell), 0x26 16x2 writing 16 x "B" and "frame N", and 0x25 64x4 writing 64 x
chr(107 + i) per row. Exits 1 when a display reported no frames, its frame count did not grow between its first and
last report (fewer than two reports included), a report comes from another address, a report does not match, a
segment holds more characters than its rows (2 x cols on a 4 row display, cols otherwise) or the simulator counted
overflow bytes or reads (the driver never reads).
"""

import pathlib
import re
import sys

DIMS = {"27": (20, 4), "26": (16, 2), "25": (64, 4)}
LINE = re.compile(
    r"LCD 0x(\w\w) frames=(\d+) cmds=(\d+) data=(\d+) cgram=(\d+) overflow=(\d+) reads=(\d+) seg0=\[(.*?)\] seg1=\[(.*?)\]"
)


def check(a: str, got: list[str]) -> str:
    c, _ = DIMS[a]
    if a == "27":
        full = all(
            len(x) == c and x[:16] == chr(97 + i) * 16 and x[16:].isdigit()
            for i, x in enumerate(got)
        )
        last = all(x == " " * c for x in got[:3]) and got[3] == " " * 19 + "x"
        return "full" if full else "x-only" if last else "BAD " + repr(got)
    if a == "26":
        ok = (
            got[0] == "B" * 16
            and got[1].startswith("frame ")
            and got[1].rstrip()[6:].isdigit()
            and len(got[1]) == 16
        )
        return "ok" if ok else "BAD " + repr(got)
    ok = all(x == chr(107 + i) * 64 for i, x in enumerate(got))
    return "ok" if ok else "BAD " + repr(got)[:80]


def main() -> int:
    rows: dict[str, list[tuple[int, int, list[str]]]] = {}
    long: dict[str, int] = {}
    overflow: dict[str, int] = {}
    reads: dict[str, int] = {}
    unknown: set[str] = set()
    for line in (
        pathlib.Path(sys.argv[1])
        .read_text(encoding="utf-8", errors="replace")
        .splitlines()
    ):
        if not (m := LINE.search(line)):
            continue
        a, frames, s0, s1 = m.group(1), int(m.group(2)), m.group(8), m.group(9)
        if a not in DIMS:
            unknown.add(a)
            continue
        if frames == 0:
            continue
        c, r = DIMS[a]
        overflow[a] = int(m.group(6))
        reads[a] = int(m.group(7))
        if max(len(s0), len(s1)) > (2 * c if r > 2 else c):
            long[a] = long.get(a, 0) + 1
        # HD44780 DDRAM: row 2 continues row 0, row 3 continues row 1
        got = [s0[:c], s1[:c], s0[c : 2 * c], s1[c : 2 * c]][:r]
        rows.setdefault(a, []).append((frames, int(m.group(5)), got))
    bad = [f"0x{a} never drew" for a in DIMS if a not in rows]
    bad += [f"0x{a} is not an expected display" for a in sorted(unknown)]
    for a, lst in rows.items():
        c, r = DIMS[a]
        kinds = [check(a, got) for _, _, got in lst]
        good = sum(not k.startswith("BAD") for k in kinds)
        print(
            f"0x{a} {c}x{r}: {good}/{len(lst)} reports match, last frame count {lst[-1][0]}, "
            f"cgram bytes {lst[-1][1]}, kinds {sorted(set(kinds))}"
        )
        print("    last:", lst[-1][2])
        if good != len(lst):
            bad.append(f"0x{a} {len(lst) - good} bad reports")
        if len(lst) < 2 or lst[-1][0] <= lst[0][0]:
            bad.append(
                f"0x{a} frame count never grew ({len(lst)} reports), the display stopped updating"
            )
        if long.get(a):
            bad.append(f"0x{a} {long[a]} reports with a segment longer than its rows")
        if overflow.get(a):
            bad.append(f"0x{a} {overflow[a]} overflow bytes")
        if reads.get(a):
            bad.append(f"0x{a} {reads[a]} reads")
    if bad:
        print("FAIL:", "; ".join(bad))
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
