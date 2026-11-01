"""compare.py BASE_CAP PR_CAP DRIVER: compare every captured window of one driver between two runs and decode it.

Exits non-zero when no window was compared, a window differs or is invalid (an SPI capture returned no result,
spi=-1, or a window with clock edges lacks its R or F bits), the PR capture has windows the base capture lacks, or
no window of the driver clocked any data (a driver that never toggled its clock would otherwise compare as SAME).
"""

import pathlib
import re
import sys


def load(path):
    caps, times, order = {}, {}, []
    cur = None
    for line in (
        pathlib.Path(path).read_text(encoding="utf-8", errors="ignore").splitlines()
    ):
        if m := re.search(
            r"CAP (\S+) (\S+) rise=(\d+) fall=(\d+) idle=(\d) drise=(\d+) dfall=(\d+) lat=(\d+)"
            r"(?: spi=(-?\d+)/(-?\d+))?",
            line,
        ):
            cur = (m[1], m[2])
            caps[cur] = {
                "rise": int(m[3]),
                "fall": int(m[4]),
                "idle": int(m[5]),
                "drise": int(m[6]),
                "dfall": int(m[7]),
                "lat": int(m[8]),
                "valid": m[9] is None or (int(m[9]) >= 0 and int(m[10]) >= 0),
            }
            order.append(cur)
        elif m := re.search(r"\]: ([RF]) (\S+) (\S+) ([0-9a-f]*)\s*$", line):
            key = (m[2], m[3])
            if key in caps:
                n = caps[key]["rise" if m[1] == "R" else "fall"]
                caps[key][m[1]] = "".join(f"{int(c, 16):04b}" for c in m[4])[:n]
        elif m := re.search(r"TIME (\S+) loop (\d+) us", line):
            times.setdefault(m[1], []).append(int(m[2]))
    return caps, times, order


def decode(drv, c):
    """Human readable frame (bits sampled on rising edges unless the driver clocks on both edges)."""
    r = c.get("R", "")
    if drv in ("bp5758d", "bp1658cj", "sm2235", "sm2335"):
        # 8 data bits and the ack clock per byte, then one clock for the stop condition
        size = (17 if drv == "bp5758d" else 12) * 9 + 1
        out = []
        for f in range(0, len(r), size):
            fr = r[f : f + size - 1]
            out.append(
                " ".join(
                    f"{int(fr[i : i + 8], 2):02x}" for i in range(0, len(fr) - 7, 9)
                )
            )
        return " | ".join(out)
    if drv == "my9231":
        f = c.get("F", "")
        first, second = (f, r) if c["idle"] == 1 else (r, f)
        bits = "".join(a + b for a, b in zip(first, second, strict=False))
        return " ".join(
            f"{int(bits[i : i + 16], 2):04x}" for i in range(0, len(bits) - 15, 16)
        )
    if drv == "tlc5947":
        return " ".join(
            f"{int(r[i : i + 12], 2):03x}" for i in range(0, len(r) - 11, 12)
        )
    if drv == "tlc5971":
        return " ".join(
            f"{int(r[i : i + 16], 2):04x}" for i in range(0, len(r) - 15, 16)
        )
    if drv == "sm16716":
        # A frame starts with 50 zero bits; anything else is shown raw.
        if "1" in r[:50]:
            return r
        return r[:50] + " | " + " ".join(r[i : i + 25] for i in range(50, len(r), 25))
    return r


base, bt, order = load(sys.argv[1])
pr, pt, _ = load(sys.argv[2])
drv = sys.argv[3]


def complete(c):
    """A window that clocked must carry its sampled bits; a lost R or F line leaves None on both sides."""
    return c["valid"] and (c["rise"] == 0 or (c.get("R") and c.get("F") is not None))


same = diff = invalid = clocked = 0
for key in order:
    if key[0] != drv:
        continue
    a, b = base[key], pr.get(key)
    if not complete(a) or (b is not None and not complete(b)):
        invalid += 1
        print(
            f"INVALID {key[1]:8s} an SPI capture returned no result or the bits line is missing"
        )
        continue
    clocked += a["rise"] > 0 and bool(a.get("R"))
    ok = b is not None and all(
        a.get(k) == b.get(k)
        for k in ("rise", "fall", "drise", "dfall", "lat", "R", "F")
    )
    same += ok
    diff += not ok
    print(
        f"{'SAME' if ok else 'DIFF'} {key[1]:11s} rise={a['rise']} drise={a['drise']} lat={a['lat']} :: {decode(drv, a)}"
    )
    if not ok:
        print(
            f"     PR {key[1]:8s} rise={b and b['rise']} drise={b and b['drise']}  {b and decode(drv, b)[:160]}"
        )
extra = [key[1] for key in pr if key[0] == drv and key not in base]
for step in extra:
    print(f"EXTRA {step:8s} only in the PR capture")
print(
    f"{drv}: {same} windows identical, {diff} different, {invalid} invalid, {len(extra)} only in the PR;"
    f" loop() us dev {bt.get(drv)} PR {pt.get(drv)}"
)
if not clocked:
    print(f"{drv}: no window clocked any data")
if same == 0 or diff or invalid or extra or not clocked:
    sys.exit(1)
