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
flagged: list[str] = []
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
        # the scanner never stopped: the phase was abandoned and later phases may scan passively
        if sc := sum("ST adv scanner_stuck" in ln for _, ln in lines):
            d["scanner_stuck"] = sc
        if not any("ST phase_end" in ln for _, ln in lines):
            d["incomplete"] = (
                True  # a reset or the end of the capture cut the adv phase short
            )
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
            r"(?: cache_clean_fail=(\d+))?(?: handle_bad=(\d+))?(?: held_dropped=(\d+))?(?: hold_failed=(\d+))?(?: send_err=(\d+))?(?: partial=(\d+))?"
            r"(?: no_readable_char=(\d+))?",
            txt,
        ):
            d["rounds,ok,first_ok,fail,stuck"] = m.groups()[:5]
            if m[6] and m[6] != "0":  # cold attempts that may have used the cache
                d["cache_clean_fail"] = int(m[6])
            if (
                m[7] and m[7] != "0"
            ):  # rounds against a wrong ff01 handle, out of ok and fail
                d["handle_bad"] = int(m[7])
            if (
                m[8] and m[8] != "0"
            ):  # third connects with a held link gone, out of ok and fail
                d["held_dropped"] = int(m[8])
            if (
                m[9] and m[9] != "0"
            ):  # rounds whose held links never came up, out of ok and fail
                d["hold_failed_rounds"] = int(m[9])
            if m[10] and m[10] != "0":  # retry writes the stack refused to send
                d["send_err"] = int(m[10])
            if m[11] and m[11] != "0":  # rounds whose RTT loop broke off, out of ok
                d["rtt_partial"] = int(m[11])
            if (
                m[12] and m[12] != "0"
            ):  # Eve rounds without a readable characteristic, out of ok and fail
                d["no_readable_char"] = int(m[12])
        elif name.split("#")[0] != "done" or "ST DONE" not in txt:
            d["incomplete"] = (
                True  # a reset or the end of the capture cut the phase short
            )
        if hm := txt.count("ST handle_mismatch"):
            d["handle_mismatch"] = hm
        ups = [int(x) for x in re.findall(r"ST conn_up \S+ r=\d+ total=(\d+)", txt)]
        d["up_total_ms"] = (med(ups), p90(ups), max(ups) if ups else None)
        # connects were started but no device Connecting line matched: the log format is not the one read here
        if "ST conn_start" in txt and not att:
            d["no_device_attempts"] = True
        # the self test only connects its three peers: an address without a label is a wrong or missing
        # PEER_ADDRS entry, and that peer's stats would be left out
        if unl := sorted({a["p"] for a in att if a["p"] not in ("L", "M", "eve")}):
            d["no_peer_label"] = unl
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
        # rounds whose RTT loop hit a send error or timeout (also when no phase_end line came)
        # also ops that completed with a GATT error status (bad=)
        if cut := sum(
            e != "0" or to != "0" or (b or "0") != "0"
            for e, to, b in re.findall(
                r"ST rtt \S+ r=\d+ .* err=(\d+) timeout=(\d+)(?: bad=(\d+))?", txt
            )
        ):
            d["rtt_partial"] = max(d.get("rtt_partial", 0), cut)
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
        d["held_failed"] = txt.count("ST third held_failed") or None
        d["hold_failed"] = (
            txt.count("ST third hold_failed") or None
        )  # h1 or h2 never settled
        nt = re.findall(
            r"ST notify min=(\d+) rx=(\d+) gaps=(\d+) missing=(\d+) drops=(\d+)(?: missing_drop=(\d+))?(?: backwards=(\d+))?"
            r"(?: dups=(\d+))?(?: short=(\d+))?(?: unheld_s=(\d+))?",
            txt,
        )
        if nt:
            d["notify last"] = nt[-1]
        if "ST notify no_ticks" in txt or (nt and nt[-1][1] == "0"):
            d["notify_no_ticks"] = True  # the phase measured nothing
        elif nt:
            # 5 ticks a second over the minutes so far; seen plus known missing well under that: the
            # subscription went quiet, as drive.py's notify_short
            mins, rx, _, missing, _, mdrop, *_ = nt[-1]
            if int(rx) + int(missing) + int(mdrop or 0) < 0.9 * int(mins) * 300:
                d["notify_short"] = True
            # the server restarted or the order broke: the gap and missing counts mean nothing
            d["notify_backwards"] = int(nt[-1][6] or 0) or None
            d["notify_dups"] = int(nt[-1][7] or 0) or None
            d["notify_short_payload"] = int(nt[-1][8] or 0) or None
            # ticks sent while the link was down count as missing_drop, so coverage alone passes a soak
            # mostly without the link; flag it as drive.py's mostly_unheld
            if int(nt[-1][9] or 0) > 0.1 * int(mins) * 60:
                d["notify_unheld_s"] = int(nt[-1][9])
        fr = [int(x) for x in re.findall(r"ST churn c=\d+ freed_ms=(\d+)", txt)]
        if fr:
            d["churn freed_ms"] = (med(fr), max(fr))
        d["retry_attempts"] = (
            dict(Counter(re.findall(r"ST retry \S+ r=\d+ attempts=(\d)", txt))) or None
        )
    d["rejected"] = sum("Rejected" in ln for _, ln in lines) or None
    d["x3e"] = sum("rsn 0x3e" in ln for _, ln in lines) or None
    print(name, {k: v for k, v in d.items() if v is not None})
    # a partial or empty measurement must not pass as a clean run, nor cold rounds that may have used the cache
    keys = (
        "incomplete",
        "notify_no_ticks",
        "handle_mismatch",
        "handle_bad",
        "scanner_stuck",
        "cache_clean_fail",
        "no_device_attempts",
        "rtt_partial",
        "held_dropped",
        "notify_short",
        "notify_backwards",
        "notify_dups",
        "notify_short_payload",
        "notify_unheld_s",
        "no_readable_char",
        "no_peer_label",
    )
    # every round failed to hold its links (or ran on a wrong handle): the phase measured nothing
    rounds = d.get("rounds,ok,first_ok,fail,stuck", ("0",))[0]
    if int(rounds) and (d.get("hold_failed_rounds", 0) + d.get("handle_bad", 0)) >= int(
        rounds
    ):
        d["nothing_measured"] = True
    keys += ("nothing_measured",)
    if bad := [k for k in keys if d.get(k)]:
        flagged.append(f"{name}: {','.join(bad)}")
# a run where the servers were never seen, or no phase started, measured nothing
txt_all = "\n".join(ln for _, ln in L)
# the done phase is only the end mark: a capture with nothing else ran no scenario
nothing = "ST not_seen" in txt_all or not [p for p in ph if p.split("#")[0] != "done"]
if not nothing and "ST DONE" not in txt_all:
    flagged.append("no ST DONE: the capture ended before the self test finished")
if "ST eve_not_seen" in txt_all:  # the Eve phases failed on the missing peer
    flagged.append(
        "eve not seen: thirdeve, thirdpd, thirdL and eve results are not about the proxy"
    )
if nothing:
    print(
        "NOTHING RUN:",
        "servers not seen (ST not_seen)"
        if "ST not_seen" in txt_all
        else "no phase started",
    )
# the boot that started the self test comes before its first ST line; a reset after that is a proxy restart,
# also one outside any phase (the wait before the first phase, or after ST DONE)
st0 = next((i for i, (_, ln) in enumerate(L) if "]: ST " in ln), len(L))
if restarts := sum("rst:" in ln for _, ln in L[st0:]):
    flagged.append(
        f"{restarts} reset lines after the self test started: the proxy restarted"
    )
if not print_heap(L, None):
    flagged.append("no HEAP lines: heap not measured")
print(
    "resets",
    sum("rst:" in ln for _, ln in L),
    "idf",
    sorted({re.search(r"idf=(\S+)", ln)[1] for _, ln in L if "idf=" in ln}),
)
print_warnings(L, 25, r"[0-9A-F]{2}(:[0-9A-F]{2}){5}|0x[0-9a-f]+|\d+")

if flagged:
    print("FLAGGED:", "; ".join(flagged))
if nothing or flagged:
    sys.exit(1)
