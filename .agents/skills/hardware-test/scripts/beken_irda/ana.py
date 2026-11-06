"""ana.py LOG: per case, how many sent frames (TX lines) decoded as what was sent (the RX lines that follow).

Exits non-zero when the log has no "CASE done" line or no frames were sent.
"""

from collections import defaultdict
import pathlib
import re
import sys


def main() -> int:
    with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines()
    state = {"case": None, "tx": None, "rx": []}
    res = defaultdict(
        lambda: [0, 0, 0]
    )  # sent, matched, frames with no protocol decode
    misses = defaultdict(list)
    done = False

    def close() -> None:
        if state["tx"] is None:
            return
        kind, want = state["tx"]
        rx = state["rx"]
        r = res[state["case"]]
        r[0] += 1
        decoded = [x for x in rx if not x.startswith("raw")]
        if kind == "repeat":
            ok = any(x.startswith("raw n=3") for x in rx)
        elif kind == "long":
            ok = True
        else:
            ok = want in decoded
        if ok:
            r[1] += 1
        else:
            misses[state["case"]].append(f"{want} -> {rx}")
        if not decoded:
            r[2] += 1

    for line in lines:
        if m := re.search(r"\[irsim.*?\]: CASE (\w+)", line):
            close()
            state.update(case=m[1], tx=None, rx=[])
            done = done or m[1] == "done"
            continue
        if m := re.search(r"\[irsim.*?\]: TX (.*)", line):
            close()
            t = m[1].replace(" glitch30", "").replace(" after_long", "")
            kind = t.split()[0]
            want = " ".join(t.split()[:3]) if kind == "nec" else t
            state.update(tx=(kind, want), rx=[])
            continue
        if m := re.search(r"\[irsim.*?\]: RX (.*)", line):
            state["rx"].append(m[1])
    close()
    sent = 0
    for c, (s, ok, nod) in res.items():
        if c == "done":
            continue
        sent += s
        print(f"{c:10} sent {s:4} ok {ok:4} no-decode {nod}")
        for miss in misses[c][:3]:
            print(f"    miss {miss}")
    if not done or not sent:
        print("FAIL: no CASE done line" if not done else "FAIL: no frames sent")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
