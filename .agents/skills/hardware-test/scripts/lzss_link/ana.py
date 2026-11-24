"""Summarise runV-s3.log and runV-hz.log of the compression test.

usage: ana.py V
Per message type and direction: messages missing from GAP lines, sent and refused counts, received ok, bad, gap and
dup, input and wire sizes, compressed share and ratio, raw packets of the wrong size, encode and decode times; then
link resets, session drops, crashes, boots and packet_interface warnings per board.
"""

import collections
from pathlib import Path
import re
import sys

R = sys.argv[1]
HERE = Path(__file__).parent


def lines(f: Path):
    with f.open(encoding="utf-8", errors="replace") as fh:
        yield from fh


def last_stats(f: Path) -> tuple[dict, dict | None]:
    st, g = {}, None
    for ln in lines(f):
        if m := re.search(r"STAT (\w) (.*)", ln):
            d = {}
            for pre, part in zip(
                ("", "w_", "e_", "d_"), m.group(2).split("|"), strict=False
            ):
                d.update(
                    (pre + k, int(v)) for k, v in re.findall(r"(\w+)=(-?\d+)", part)
                )
            st[m.group(1)] = d
        if "STATG" in ln:
            g = {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", ln)}
    return st, g


def rx_lines(f: Path) -> dict:
    d = collections.defaultdict(list)
    for ln in lines(f):
        if m := re.search(r"RX (\w) seq=\d+ in=(\d+) wire=(\d+) (\w+)", ln):
            d[m.group(1)].append((int(m.group(2)), int(m.group(3)), m.group(4)))
    return d


def missing(f: Path) -> collections.Counter:
    d = collections.Counter()
    for ln in lines(f):
        if m := re.search(r"GAP (\w) (-?\d+) -> (\d+)", ln):
            d[m.group(1)] += (int(m.group(3)) - int(m.group(2)) - 1) % 65536
    return d


def count(f: Path, pat: str) -> int:
    return sum(1 for ln in lines(f) if re.search(pat, ln))


L = {b: HERE / f"run{R}-{b}.log" for b in ("s3", "hz")}
S = {b: last_stats(L[b]) for b in L}
for snd, rcv in (("s3", "hz"), ("hz", "s3")):
    print(f"--- {snd} -> {rcv}")
    tx, rx = S[snd][0], S[rcv][0]
    rxl, miss = rx_lines(L[rcv]), missing(L[rcv])
    for t in "SWDHBX":
        a, b = tx.get(t, {}), rx.get(t, {})
        if not a and not b:
            continue
        rl = rxl.get(t, [])
        ins = [x[0] for x in rl]
        outs = [x[1] for x in rl]
        comp = sum(1 for x in rl if x[2] == "comp")
        ratio = (sum(outs) / sum(ins)) if ins else 0
        print(
            f"{t}: missing={miss.get(t, 0)} tx={a.get('tx')} ref={a.get('ref')} | rx ok={b.get('ok')}"
            f" bad={b.get('bad')} gap={b.get('gap')} dup={b.get('dup')}"
            f" | in {min(ins, default=0)}-{max(ins, default=0)} wire {min(outs, default=0)}-{max(outs, default=0)}"
            f" comp={comp}/{len(rl)} ratio={ratio:.2f} rawbad={b.get('w_rawbad')}"
            f" | enc avg={a.get('e_avg')} max={a.get('e_max')} us | dec avg={b.get('d_avg')} max={b.get('d_max')}"
        )
    print("G", S[rcv][1])
for b, f in L.items():
    print(
        b,
        "resets:",
        count(f, r"resetting link"),
        "sess down:",
        count(f, "SESSION down"),
        "crash:",
        count(
            f,
            r"Guru|panic|Exception|rst cause:[2-4]|wdt reset|abort\(\)|Soft WDT|Backtrace",
        ),
        "boot:",
        count(f, "BEGIN"),
        "W lines:",
        count(f, r"\[W\]\[packet_interface"),
        "refused-warn:",
        count(f, r"does not compress|is larger than"),
    )
