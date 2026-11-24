"""evring.py CAPTURE alt0|notif: read the boot recorder's event ring (EV lines printed on the next boot).

alt0: per recorded boot, every alt 1 to alt 0 switch with the next netd_reset, next alt 1 and first IN transfer.
notif: per recorded boot, times after alt 1 of the speed and connection notifications (IN 0x84 completes of 16 and 8
bytes), the first alt 0 and the first NCM IN transfer start.
"""

import pathlib
import re
import sys

EV = re.compile(r"EV \d+ t=([\d.]+) ms code=(\w\w) a=(\w\w) b=(\w{4})")


def blocks(path: str) -> list[dict]:
    out: list[dict] = []
    cur: dict | None = None
    with pathlib.Path(path).open(errors="replace", encoding="utf-8") as f:
        for line in f:
            if "EVENTS " in line and "recorded" in line:
                cur = {"ts": line[1:9], "ev": []}
                out.append(cur)
                continue
            if (m := EV.search(line)) and cur is not None:
                cur["ev"].append((float(m[1]), m[2], m[3], m[4]))
    return out


def alt0(b: dict) -> None:
    ev = b["ev"]
    alt1 = None
    out = []
    for i, (t, c, a, v) in enumerate(ev):
        if c == "48" and a == "0B" and v == "0001":
            alt1 = t
        if c == "48" and a == "0B" and v == "0000" and alt1 is not None:
            rest = ev[i + 1 :]
            nxt = [e for e in rest if e[1] == "48" and e[2] == "0B" and e[3] == "0001"]
            rst = [e for e in rest if e[1] == "10"]
            fin = [e for e in rest if e[1] == "20" and e[2] == "83"]
            out.append(
                f"alt1 {alt1:.1f} -> alt0 {t:.1f} (+{t - alt1:.1f} ms); "
                f"next netd_reset {f'{rst[0][0]:.0f}' if rst else '-'}; "
                f"next alt1 {f'{nxt[0][0]:.0f}' if nxt else '-'}; "
                f"first IN after {f'{fin[0][0]:.0f}' if fin else '-'}"
            )
    if out:
        print(b["ts"], *out, sep="\n  ")


def notif(b: dict, seen: set) -> None:
    r: dict[str, float] = {}
    for t, c, a, v in b["ev"]:
        if c == "48" and a == "0B" and v == "0001":
            r.setdefault("alt1", t)
        if c == "48" and a == "0B" and v == "0000":
            r.setdefault("alt0", t)
        if c == "13" and a == "84" and v == "0010":
            r.setdefault("spd", t)
        if c == "13" and a == "84" and v == "0008":
            r.setdefault("con", t)
        if c == "20" and a == "83":
            r.setdefault("in", t)
    key = (r.get("alt1"), r.get("spd"))
    if key in seen or "alt1" not in r:
        return
    seen.add(key)

    def rel(x: str) -> str:
        return f"{r[x] - r['alt1']:7.1f}" if x in r else "      -"

    print(
        b["ts"],
        f"alt1={r['alt1']:7.1f}",
        "spd",
        rel("spd"),
        "con",
        rel("con"),
        "alt0",
        rel("alt0"),
        "firstIN",
        rel("in"),
    )


def main(path: str, mode: str) -> None:
    seen: set = set()
    for b in blocks(path):
        if mode == "alt0":
            alt0(b)
        else:
            notif(b, seen)


main(sys.argv[1], sys.argv[2])
