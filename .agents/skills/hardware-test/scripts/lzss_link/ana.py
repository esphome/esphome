"""Summarise runV-s3.log and runV-hz.log of the compression test.

usage: ana.py V
Per message type and direction: messages missing from GAP lines, sender restarts (a GAP back to a lower seq), sent and refused counts, received ok, bad, gap and
dup, input and wire sizes, compressed share and ratio, raw packets of the wrong size, encode and decode times; then
link resets, session drops, crashes, boots and packet_interface warnings per board.
"""

import collections
from pathlib import Path
import re
import sys

R = sys.argv[1]
HERE = Path(__file__).parent


STAT_RE = re.compile(r"STAT (\w) (.*)")
KV_RE = re.compile(r"(\w+)=(-?\d+)")
RX_RE = re.compile(r"RX (\w) seq=\d+ in=(\d+) wire=(\d+) (\w+)")
GAP_RE = re.compile(r"GAP (\w) (-?\d+) -> (\d+)")
COUNTS = {
    "resets": re.compile(r"resetting link"),
    "sess down": re.compile("SESSION down"),
    "crash": re.compile(
        r"Guru|panic|Exception|rst cause:[2-4]|wdt reset|abort\(\)|Soft WDT|Backtrace"
    ),
    "boot": re.compile("BEGIN"),
    "W lines": re.compile(r"\[W\]\[packet_interface"),
    "refused-warn": re.compile(r"does not compress|is larger than"),
}


def parse(f: Path) -> dict:
    """One pass over a log: last STAT per type, last STATG, RX sizes, GAP misses and the COUNTS matches."""
    st, g = {}, None
    rx, miss, restarts, cnt = (
        collections.defaultdict(list),
        collections.Counter(),
        collections.Counter(),
        collections.Counter(),
    )
    with f.open(encoding="utf-8", errors="replace") as fh:
        for ln in fh:
            if m := STAT_RE.search(ln):
                d = {}
                for pre, part in zip(
                    ("", "w_", "e_", "d_"), m.group(2).split("|"), strict=False
                ):
                    d.update((pre + k, int(v)) for k, v in KV_RE.findall(part))
                st[m.group(1)] = d
            if "STATG" in ln:
                g = {k: int(v) for k, v in KV_RE.findall(ln)}
            if m := RX_RE.search(ln):
                rx[m.group(1)].append((int(m.group(2)), int(m.group(3)), m.group(4)))
            if m := GAP_RE.search(ln):
                old, new = int(m.group(2)), int(m.group(3))
                if new < old and old - new < 32768:
                    restarts[m.group(1)] += 1
                else:
                    miss[m.group(1)] += (new - old - 1) % 65536
            cnt.update(k for k, r in COUNTS.items() if r.search(ln))
    return {"st": st, "g": g, "rx": rx, "miss": miss, "restarts": restarts, "cnt": cnt}


L = {b: HERE / f"run{R}-{b}.log" for b in ("s3", "hz")}
P = {b: parse(L[b]) for b in L}
for snd, rcv in (("s3", "hz"), ("hz", "s3")):
    print(f"--- {snd} -> {rcv}")
    tx, rx = P[snd]["st"], P[rcv]["st"]
    rxl, miss, rst = P[rcv]["rx"], P[rcv]["miss"], P[rcv]["restarts"]
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
            f"{t}: missing={miss.get(t, 0)} restarts={rst.get(t, 0)} tx={a.get('tx')} ref={a.get('ref')} | rx ok={b.get('ok')}"
            f" bad={b.get('bad')} gap={b.get('gap')} dup={b.get('dup')}"
            f" | in {min(ins, default=0)}-{max(ins, default=0)} wire {min(outs, default=0)}-{max(outs, default=0)}"
            f" comp={comp}/{len(rl)} ratio={ratio:.2f} rawbad={b.get('w_rawbad')}"
            f" | enc avg={a.get('e_avg')} max={a.get('e_max')} us | dec avg={b.get('d_avg')} max={b.get('d_max')}"
        )
    print("G", P[rcv]["g"])
for b in L:
    print(b, *(f"{k}: {P[b]['cnt'][k]}" for k in COUNTS))
