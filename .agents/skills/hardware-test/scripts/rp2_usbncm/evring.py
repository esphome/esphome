"""evring.py CAPTURE alt0|notif|count: read the boot recorder's event ring (EV lines printed on the next boot).

alt0: per recorded boot, every alt 1 to alt 0 switch with the next netd_reset, next alt 1 and first IN transfer.
notif: per recorded boot, times after alt 1 of the speed and connection notifications (IN 0x84 completes of 16 and 8
bytes), the first alt 0 and the first NCM IN transfer start.
count: the number of alt 0 requests in the first ring of the capture (cycle.sh prints it per boot); prints NA and
exits 1 when the capture has no ring.
"""

import pathlib
import re
import sys

EV = re.compile(r"\bEV \d+ t=([\d.]+) ms code=(\w\w) a=(\w\w) b=(\w{4})")


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


def is_alt(e: tuple, n: int) -> bool:
    """SET_INTERFACE request to alternate setting N."""
    return e[1] == "48" and e[2] == "0B" and e[3] == f"{n:04X}"


def first(rest: list, pred) -> str:
    return next((f"{e[0]:.0f}" for e in rest if pred(e)), "-")


def alt0(b: dict) -> None:
    ev = b["ev"]
    alt1 = None
    out = []
    for i, e in enumerate(ev):
        t = e[0]
        if is_alt(e, 1):
            alt1 = t
        if is_alt(e, 0) and alt1 is not None:
            rest = ev[i + 1 :]
            out.append(
                f"alt1 {alt1:.1f} -> alt0 {t:.1f} (+{t - alt1:.1f} ms); "
                f"next netd_reset {first(rest, lambda x: x[1] == '10')}; "
                f"next alt1 {first(rest, lambda x: is_alt(x, 1))}; "
                f"first IN after {first(rest, lambda x: x[1] == '20' and x[2] == '83')}"
            )
    if out:
        print(b["ts"], *out, sep="\n  ")


def notif(b: dict) -> None:
    r: dict[str, float] = {}
    for e in b["ev"]:
        t, c, a, v = e
        if is_alt(e, 1):
            r.setdefault("alt1", t)
        if is_alt(e, 0):
            r.setdefault("alt0", t)
        if c == "13" and a == "84" and v == "0010":
            r.setdefault("spd", t)
        if c == "13" and a == "84" and v == "0008":
            r.setdefault("con", t)
        if c == "20" and a == "83":
            r.setdefault("in", t)
    if "alt1" not in r:
        return

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
    rings = blocks(path)
    if mode == "count":
        if not rings:
            print("NA")
            sys.exit(1)
        print(sum(is_alt(e, 0) for e in rings[0]["ev"]))
        return
    seen: set = set()
    for b in rings:
        key = tuple(b["ev"])
        if key in seen:  # the same boot's ring, printed again by the next report
            continue
        seen.add(key)
        if mode == "alt0":
            alt0(b)
        else:
            notif(b)


main(sys.argv[1], sys.argv[2])
