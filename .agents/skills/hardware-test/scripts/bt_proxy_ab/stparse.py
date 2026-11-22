"""stparse.py CAPTURE: summary of a selftest.hpp run (classic ESP32 on-device scenarios)."""

from collections import Counter, defaultdict
from functools import partial
import pathlib
import re
import statistics
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import capstats  # noqa: E402
from capstats import print_heap, print_warnings, sec, unwrap  # noqa: E402
from peers import addrs  # noqa: E402

ADDR = addrs()  # PEER_ADDRS=L=MAC,M=MAC,eve=MAC
if not ADDR:
    print(
        "PEER_ADDRS is not set: the per-peer device stats stay empty", file=sys.stderr
    )
med = partial(capstats.med, nd=None)  # whole numbers here
p90 = partial(capstats.p90, nd=None)


L = []
for ln in (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
):
    m = re.match(r"\[(\d\d:\d\d:\d\d\.\d+)\]", ln)
    if m:
        L.append((sec(m[1]), ln.rstrip()))
if not L:
    sys.exit(
        "no lines with millisecond time stamps (capture with serial_capture.py --ms)"
    )
L = list(zip(unwrap([t for t, _ in L]), [ln for _, ln in L], strict=True))
phase, ph, seen = None, defaultdict(list), Counter()
for t, ln in L:
    if m := re.search(r"ST phase_start (\w+)", ln):
        # after a reset the self test starts over: a repeated phase gets its own bucket (adv#2)
        seen[m[1]] += 1
        phase = m[1] if seen[m[1]] == 1 else f"{m[1]}#{seen[m[1]]}"
    if phase:
        ph[phase].append((t, ln))
for name, lines in ph.items():
    d = {}
    att, cur = [], {}
    for t, ln in lines:
        if m := re.search(r"\]: \[(\d)\] \[([0-9A-F:]{17})\] 0x\w+ Connecting", ln):
            # IVLOG names a link by the last 5 characters of its address
            sfx = m[2][-5:] if m[2] in ADDR else None
            cur[m[1]] = {"t0": t, "p": ADDR.get(m[2], m[2]), "sfx": sfx}
            att.append(cur[m[1]])
        elif m := re.search(
            r"\]: \[(\d)\] (?:\[[0-9A-F:]{17}\] )?Connection open (?:error|failed), status=(\d+)",
            ln,
        ):
            if a := cur.get(m[1]):
                a["fail"] = int(m[2])
        elif m := re.search(
            r"\]: \[(\d)\] (?:\[[0-9A-F:]{17}\] )?Connection open(?! error| failed)", ln
        ):
            if (a := cur.get(m[1])) and "to" not in a:
                a["to"] = t
        elif m := re.search(r"IVLOG connect (\S+) if=\d+ interval=(\d+)", ln):
            for a in cur.values():
                if "iv" not in a and a["sfx"] == m[1]:
                    a["iv"] = int(m[2])
        elif m := re.search(
            r"\]: \[(\d)\] \[[0-9A-F:]{17}\] Service discovery complete", ln
        ):
            if (a := cur.get(m[1])) and "td" not in a:
                a["td"] = t
        elif m := re.search(
            r"\]: \[(\d)\] \[[0-9A-F:]{17}\] Disconnected, reason=(0x\w+)|\]: \[(\d)\] \[[0-9A-F:]{17}\] ESP_GATTC_DISCONNECT_EVT, reason (0x\w+)",
            ln,
        ):
            k = m[1] or m[3]
            if a := cur.pop(k, None):
                a["end"], a["te"] = m[2] or m[4], t
    if name.split("#")[0] == "adv":
        for mode in ("active", "passive"):
            v = [
                int(x) / 10
                for x in re.findall(
                    rf"ST adv mode={mode} n10=(\d+)", "\n".join(ln for _, ln in lines)
                )
            ]
            d[mode] = (
                f"{round(statistics.mean(v))}/s ({min(v):.0f}-{max(v):.0f}) n={len(v)}"
                if v
                else None
            )
    else:
        txt = "\n".join(ln for _, ln in lines)
        if m := re.search(
            r"ST phase_end \w+ rounds=(\d+) ok=(\d+) first_ok=(\d+) fail=(\d+) stuck=(\d+)"
            r"(?: cache_clean_fail=(\d+))?(?: handle_bad=(\d+))?",
            txt,
        ):
            d["rounds,ok,first_ok,fail,stuck"] = m.groups()[:5]
        else:  # a reset or the end of the capture cut the phase short
            d["incomplete"] = True
            if m[6] and m[6] != "0":  # cold attempts that may have used the cache
                d["cache_clean_fail"] = int(m[6])
            if (
                m[7] and m[7] != "0"
            ):  # rounds against a wrong ff01 handle, out of ok and fail
                d["handle_bad"] = int(m[7])
        if hm := txt.count("ST handle_mismatch"):
            d["handle_mismatch"] = hm
        ups = [int(x) for x in re.findall(r"ST conn_up \S+ r=\d+ total=(\d+)", txt)]
        d["up_total_ms"] = (med(ups), p90(ups), max(ups) if ups else None)
        for pe in ("L", "M", "eve"):
            A = [a for a in att if a["p"] == pe]
            if not A:
                continue
            link = [1000 * (a["to"] - a["t0"]) for a in A if "to" in a]
            disc = [1000 * (a["td"] - a["to"]) for a in A if "td" in a and "to" in a]
            d[pe] = {
                "n": len(A),
                "linked": len(link),
                "link_ms": med(link),
                "link_p90": p90(link),
                "disc_n": len(disc),
                "disc_ms": med(disc),
                "disc_p90": p90(disc),
                "iv": dict(Counter(a.get("iv") for a in A)),
                "ends": dict(Counter(a.get("end") for a in A)),
            }
            if pe == "eve":
                d[pe]["drop_after_open_ms"] = med(
                    [
                        1000 * (a["te"] - a["to"])
                        for a in A
                        if a.get("end") == "0x13" and "to" in a
                    ]
                )
        rr = re.findall(
            r"ST rtt \S+ r=\d+ read_avg=(\d+) write_avg=(\d+) n=(\d)/(\d)", txt
        )
        if rr:
            # a round where every read or write failed logs an average of 0: leave it out, count it
            d["rtt read/write ms"] = (
                med([int(a) for a, _, rn, _ in rr if rn != "0"]),
                med([int(b) for _, b, _, wn in rr if wn != "0"]),
            )
            d["rtt rounds without read/write"] = (
                sum(rn == "0" for _, _, rn, _ in rr),
                sum(wn == "0" for *_, wn in rr),
            )
        th = re.findall(r"ST third \S+ r=\d+ result=(-?\d) t=(\d+) held=(\d)(\d)", txt)
        if th:
            d["third results"] = dict(Counter(r for r, *_ in th))
            d["third ok t_ms"] = med([int(t) for r, t, *_ in th if r == "1"])
            d["held both"] = sum(a == "1" and b == "1" for *_, a, b in th)
        d["held_failed"] = txt.count("held_failed") or None
        d["hold_failed"] = txt.count("hold_failed") or None  # h1 or h2 never settled
        nt = re.findall(
            r"ST notify min=(\d+) rx=(\d+) gaps=(\d+) missing=(\d+) drops=(\d+)(?: missing_drop=(\d+))?(?: backwards=(\d+))?",
            txt,
        )
        if nt:
            d["notify last"] = nt[-1]
        fr = [int(x) for x in re.findall(r"ST churn c=\d+ freed_ms=(\d+)", txt)]
        if fr:
            d["churn freed_ms"] = (med(fr), max(fr))
        d["retry_attempts"] = (
            dict(Counter(re.findall(r"ST retry \S+ r=\d+ attempts=(\d)", txt))) or None
        )
    d["rejected"] = sum("Rejected" in ln for _, ln in lines) or None
    d["x3e"] = sum("rsn 0x3e" in ln for _, ln in lines) or None
    print(name, {k: v for k, v in d.items() if v is not None})
print_heap(L, None)
print(
    "resets",
    sum("rst:" in ln for _, ln in L),
    "idf",
    sorted({re.search(r"idf=(\S+)", ln)[1] for _, ln in L if "idf=" in ln}),
)
print_warnings(L, 25, r"[0-9A-F]{2}(:[0-9A-F]{2}){5}|0x[0-9a-f]+|\d+")
