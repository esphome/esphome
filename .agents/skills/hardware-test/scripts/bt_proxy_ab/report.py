"""report.py TAG: per step of a suite run, driver results (TAG-<step>.jsonl) and device-side times from cap-TAG.txt."""

from collections import Counter
import json
import pathlib
import re
import statistics
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from capstats import med, p90, print_heap, print_warnings, sec, unwrap  # noqa: E402
from peers import addrs  # noqa: E402

T = sys.argv[1]
ADDR = addrs()  # PEER_ADDRS=L=MAC,M=MAC,eve=MAC
if not ADDR:
    print(
        "PEER_ADDRS is not set: the per-peer device stats stay empty", file=sys.stderr
    )


lines = []
for ln in (
    pathlib.Path(f"cap-{T}.txt")
    .read_text(encoding="utf-8", errors="replace")
    .splitlines()
):
    m = re.match(r"\[(\d\d:\d\d:\d\d(?:\.\d+)?)\]", ln)
    if m:
        lines.append((sec(m[1]), ln.rstrip("\n")))
if not lines:
    sys.exit(
        f"no time stamped lines in cap-{T}.txt (capture with --ms or apilogcap.py)"
    )
lines = list(zip(unwrap([t for t, _ in lines]), [ln for _, ln in lines], strict=True))

# device attempts
att, cur = [], {}
for t, ln in lines:
    if m := re.search(
        r"\]: \[(\d)\] \[([0-9A-F:]{17})\] Connecting v3 (with|without) cache", ln
    ):
        cur[m[1]] = {
            "t0": t,
            "peer": ADDR.get(m[2], m[2]),
            "sfx": m[2][-5:],
            "cached": m[3] == "with",
            "x3e": 0,
        }
        att.append(cur[m[1]])
    # the address is in the line on dev and not with the Bluedroid proxy backend
    elif m := re.search(
        r"\]: \[(\d)\] (?:\[[0-9A-F:]{17}\] )?Connection open(?! error| failed)", ln
    ):
        if (a := cur.get(m[1])) and "t_open" not in a:
            a["t_open"] = t
    elif m := re.search(r"IVLOG connect (\S+) if=\d+ interval=(\d+)", ln):
        for a in cur.values():
            if a["sfx"] == m[1] and "iv" not in a:
                a["iv"] = int(m[2])
    elif m := re.search(r"IVLOG update (\S+) status=(\d+) interval=(\d+)", ln):
        for a in cur.values():
            if a["sfx"] == m[1]:
                a.setdefault("upd", []).append(int(m[3]))
    elif m := re.search(
        r"\]: \[(\d)\] (?:\[[0-9A-F:]{17}\] )?Service discovery complete", ln
    ):
        if (a := cur.get(m[1])) and "t_disc" not in a:
            a["t_disc"] = t
    elif m := re.search(
        r"\]: \[(\d)\] \[[0-9A-F:]{17}\] Disconnected, reason=(0x\w+)", ln
    ):
        if a := cur.pop(m[1], None):
            a["end"], a["t_end"] = m[2], t
    elif m := re.search(
        r"\]: \[(\d)\] (?:\[[0-9A-F:]{17}\] )?Connection open (?:error|failed), status=(\d+)",
        ln,
    ):
        if a := cur.get(m[1]):
            a["fail"] = int(m[2])
    elif "rsn 0x3e" in ln:
        for a in cur.values():
            if "t_open" not in a:
                a["x3e"] += 1

marks = []
for ln in pathlib.Path(f"marks-{T}.txt").read_text(encoding="utf-8").splitlines():
    hms, rest = ln.split(" ", 1)
    marks.append((sec(hms), rest.strip()))
marks = list(
    zip(
        unwrap([t for t, _ in marks], lines[0][0] if lines else None),
        [r for _, r in marks],
        strict=True,
    )
)
# every start/end interval of a step (a resumed run adds more); an interrupted one ends at the next start
steps: dict[str, list[list]] = {}
open_iv = None
# suite.sh's own verdict: the last driver rc per step and the incomplete list; None: no verdict after the
# last start, so the run was killed or is still going
last_rc: dict[str, int] = {}
suite_incomplete: str | None = None
ended: dict[str, bool] = {}  # False: the step's last run has no end mark
# per step, from its last run: a RESUME run's suite done only covers the steps it ran
step_result: dict[str, str] = {}
dead_bg: set[str] = (
    set()
)  # a capture or pinger that died in any run left a hole in that run's data
for t, r in marks:
    if m := re.match(r"(\S+) chunk \d+ rc=(\d+)", r):
        last_rc[m[1]] = int(m[2])
    elif m := re.match(r"result (\S+) (\S+)$", r):
        step_result[m[1]] = m[2]
    elif r.startswith("suite incomplete:"):
        suite_incomplete = r.split(":", 1)[1].strip()
        dead_bg |= {"cap", "ping"} & set(suite_incomplete.split())
    elif r == "suite done":  # a later RESUME run finished the rest
        suite_incomplete = ""
    elif r.startswith("start "):
        suite_incomplete = None
        ended[r[6:]] = False
    elif r.startswith("end "):
        ended[r[4:]] = True
    if r.startswith("start "):
        if open_iv:
            open_iv[1] = t
        open_iv = [t, None]
        steps.setdefault(r[6:], []).append(open_iv)
    elif r.startswith("end ") and open_iv:
        open_iv[1] = t
        open_iv = None


def window(s):
    def inside(t):
        return any(a <= t <= (b or 1e9) for a, b in steps[s])

    return [x for x in att if inside(x["t0"])], [ln for t, ln in lines if inside(t)]


# harness faults: kept out of the ratios and counted on their own
HARNESS = ("clear_failed", "no_ff01", "stale_handles", "no_readable_char")


def jl(s):
    try:
        return [
            json.loads(x)
            for x in pathlib.Path(f"{T}-{s}.jsonl")
            .read_text(encoding="utf-8")
            .splitlines()
        ]
    except FileNotFoundError:
        print(f"{s}: no {T}-{s}.jsonl, the driver wrote nothing")
        flagged.append(f"{s}: no driver file")
        return []


flagged: list[str] = []


out = {}
for s in steps:
    A, L = window(s)
    R = jl(s)
    # a failed cache clear is a harness fault: out of the ratios, counted on its own
    rounds = [r for r in R if "round" in r and r.get("result") not in HARNESS]
    d = {}
    if ab := len(list(pathlib.Path().glob(f"{T}-{s}.aborted*.jsonl"))):
        d["aborted_runs"] = (
            ab  # partial records of a run that was retried, left out of the numbers
        )
    for h in HARNESS:
        if n := sum(r.get("result") == h for r in R):
            d[h] = n
    # results outside any round (not_seen, api_lost, connect_failed, no_handles) would otherwise vanish
    if other := Counter(r["result"] for r in R if "round" not in r and "result" in r):
        d["no_round"] = dict(other)
    if s == "adv":
        for r in R:
            if (
                r.get("test") == "adv" and "mode" in r
            ):  # restore_not_applied is in no_round
                d[r["mode"]] = (
                    f"{r['mean']}/s ({r['min10']}-{r['max10']}), {r['unique']} addr"
                    + (
                        ""
                        if r.get("applied", True)
                        else f", MODE NOT APPLIED {r['scanner']}"
                    )
                )
    elif s.startswith(("disc", "cache")):
        d["ok"] = f"{sum(r['ok'] for r in rounds)}/{len(rounds)}"
        d["api_conn_s"] = med([r.get("t_conn") for r in rounds])
        d["api_svc_s"] = med([r.get("t_svc") for r in rounds if s.startswith("disc")])
        for k in ("read_ms", "write_ms", "notify_ms"):
            d[k] = med([r.get(k) for r in rounds])
        dev = [a for a in A if a["peer"] == ("L" if s.endswith("L") else "M")]
        d["dev_n"] = len(dev)
        d["dev_link_ms"] = med(
            [1000 * (a["t_open"] - a["t0"]) for a in dev if "t_open" in a]
        )
        d["dev_link_p90"] = p90(
            [1000 * (a["t_open"] - a["t0"]) for a in dev if "t_open" in a]
        )
        if s.startswith("disc"):
            dd = [
                1000 * (a["t_disc"] - a["t_open"])
                for a in dev
                if "t_disc" in a and "t_open" in a
            ]
            d["dev_disc_ms"], d["dev_disc_p90"], d["dev_disc_max"] = (
                med(dd),
                p90(dd),
                round(max(dd)) if dd else None,
            )
        d["iv"] = dict(Counter(a.get("iv") for a in dev))
        d["upd"] = dict(Counter(str(a.get("upd", [])) for a in dev))
        d["x3e"] = sum(a["x3e"] for a in dev)
        d["tables"] = dict(Counter(str(r.get("n")) for r in rounds if r.get("n")))
        d["exc"] = Counter(
            (r.get("exc") or r.get("rtt_exc") or "")[:50]
            for r in rounds
            if r.get("exc") or r.get("rtt_exc") or r.get("timeout")
        )
    elif s.startswith("retry"):
        d["first_ok"] = f"{sum(r['first_ok'] for r in rounds)}/{len(rounds)}"
        d["ok"] = f"{sum(r['ok'] for r in rounds)}/{len(rounds)}"
        d["attempts_mean"] = (
            round(statistics.mean(r["attempts"] for r in rounds), 2) if rounds else None
        )
        d["t_cmd_first"] = med([r["detail"][0].get("t_cmd") for r in rounds])
    elif s.startswith("third"):
        th = [r for r in rounds if "third" in r]
        # a held link that dropped while the third connected: only two links were up
        d["held_dropped"] = sum(not all(r.get("held_ok", [True])) for r in th)
        th = [r for r in th if all(r.get("held_ok", [True]))]
        d["rounds"] = len(rounds)
        d["held_failed"] = sum(r.get("result") == "held_failed" for r in rounds)
        if rounds and d["held_failed"] >= len(
            rounds
        ):  # no round held its links: nothing measured
            d["all_held_failed"] = True
        # a third link that connected but could not discover is not a third-link success
        d["svc_failed"] = sum(bool(r.get("connected") and r.get("svc_exc")) for r in th)
        d["connected"] = sum(
            bool(r.get("connected") and not r.get("svc_exc")) for r in th
        )
        d["t_svc_after"] = med([r.get("t_svc_after") for r in th])
        d["errs"] = dict(Counter(r.get("err") for r in th if not r.get("connected")))
        d["api_conn_s"] = med(
            [r.get("t_conn") for r in th if r.get("connected") or r.get("err") == 19]
        )
        peer = {"thirdeve": "eve", "thirdL": "L"}.get(s, "M")
        dev = [a for a in A if a["peer"] == peer and not a["cached"]]
        d["dev_attempts"] = len(dev)
        d["dev_linked"] = sum("t_open" in a for a in dev)
        d["dev_link_ms"] = med(
            [1000 * (a["t_open"] - a["t0"]) for a in dev if "t_open" in a]
        )
        d["dev_fail"] = dict(Counter(a.get("fail") for a in dev if "fail" in a))
        d["iv"] = dict(Counter(a.get("iv") for a in dev))
        d["x3e"] = sum(a["x3e"] for a in dev)
    elif s == "notify":
        for r in R:
            if (
                r.get("test") == "notify" and "result" not in r
            ):  # no_handles is in no_round
                d.update(
                    {
                        k: r.get(k)
                        for k in (
                            "secs",
                            "expected",
                            "rx",
                            "gaps",
                            "missing",
                            "missing_drop",
                            "backwards",
                            "dups",
                            "short",
                            "reconnects",
                            "subscribe_errors",
                            "hold_failed",
                            "unheld_s",
                        )
                    }
                )
                d["drops"] = r.get("drops")
        if "rx" not in d:
            d.setdefault("warn", []).append("no notify summary record")
    elif s == "churn":
        for r in R:
            if (
                r.get("test") == "churn" and "result" not in r
            ):  # no_free_state is in no_round
                d.update(
                    {
                        k: r.get(k)
                        for k in (
                            "cycles",
                            "failed",
                            "stuck",
                            "limit",
                            "free_end",
                            "slot_not_seen",
                        )
                    }
                )
        if "cycles" not in d:
            d.setdefault("warn", []).append("no churn summary record")
        cyc = [r for r in R if r.get("test") == "churn_cycle"]
        d["t_conn_med"] = med([r.get("t_conn") for r in cyc])
        d["t_free_med"] = med([r.get("t_free") for r in cyc])
    elif s == "eve":
        d["ok"] = f"{sum(r.get('ok', False) for r in rounds)}/{len(rounds)}"
        d["errs"] = dict(
            Counter(r.get("err") for r in rounds if not r.get("connected"))
        )
        d["api_svc_s"] = med([r.get("t_svc") for r in rounds])
        dev = [a for a in A if a["peer"] == "eve"]
        d["dev_link_ms"] = med(
            [1000 * (a["t_open"] - a["t0"]) for a in dev if "t_open" in a]
        )
        dd = [
            1000 * (a["t_disc"] - a["t_open"])
            for a in dev
            if "t_disc" in a and "t_open" in a
        ]
        d["dev_disc_done"] = f"{len(dd)}/{len(dev)}"
        d["dev_disc_ms"] = med(dd)
        d["drop_after_ms"] = med(
            [
                1000 * (a["t_end"] - a["t_open"])
                for a in dev
                if "t_end" in a and "t_open" in a and a.get("end") == "0x13"
            ]
        )
        d["iv"] = dict(Counter(a.get("iv") for a in dev))
        d["upd"] = dict(Counter(str(a.get("upd", [])) for a in dev))
    # a round step whose driver counted no rounds (a crash in every chunk) measured nothing
    if s.startswith(("disc", "cache", "retry", "third", "eve")) and not rounds:
        d.setdefault("warn", []).append("no counted rounds")
    # driver rounds but no device connect line: the log format is not the one these patterns read
    # a device attempt without a label: PEER_ADDRS is wrong or misses a peer, whose stats are left out
    peer_steps = ("disc", "cache", "third", "eve")
    if (
        rounds
        and s.startswith(peer_steps)
        and (
            unl := sorted({a["peer"] for a in A if a["peer"] not in ("L", "M", "eve")})
        )
    ):
        d.setdefault("warn", []).append(
            f"device attempts without a PEER_ADDRS label: {', '.join(unl)}"
        )
    if rounds and not A and s != "adv":
        d.setdefault("warn", []).append(
            "driver rounds but no device Connecting line matched"
        )
    if partial := sum(bool(r.get("rtt_partial")) for r in rounds):
        d["rtt_partial"] = partial
    if A and not any("t_open" in a for a in A):
        d.setdefault("warn", []).append(
            "device attempts but no Connection open line matched"
        )
    d["rejected"] = sum("Rejected" in ln for ln in L)
    d["tcp_full"] = sum("TCP buffer full" in ln for ln in L)
    out[s] = d
    # a partial or faulted step must not pass as a clean result
    # rtt_partial: RTT medians from rounds whose RTT loop broke off
    # backwards, dups, short: the tick order broke, so the notify gap and missing counts mean nothing
    # subscribe_errors: notify passes whose tick read or subscribe failed on a held link (unheld time is
    # mostly_unheld in no_round)
    # slot_not_seen: churn cycles whose taken slot never showed, so their freed reading is not a measurement
    if bad := [
        k
        for k in (
            "warn",
            "no_round",
            "aborted_runs",
            "rtt_partial",
            "held_dropped",
            "all_held_failed",
            "backwards",
            "dups",
            "short",
            "slot_not_seen",
            "subscribe_errors",
            *HARNESS,
        )
        if d.get(k)
    ]:
        flagged.append(f"{s}: {','.join(bad)}")
    print(s, json.dumps(d, default=str))

# whole run
if not print_heap(lines):
    flagged.append("no HEAP lines: heap not measured")
boots = sum(
    ("Reset reason" in ln or "rst:" in ln or "ESP-ROM" in ln) for _, ln in lines
)
# a capture over the API (apilogcap.py) never sees ROM or early boot lines: 0 there does not mean no reboot
gaps = sum("APILOGCAP CONNECT FAILED" in ln for _, ln in lines)
# a reboot that reconnects at once leaves no failed connect, only the lost session
lost_sessions = sum("APILOGCAP SESSION LOST" in ln for _, ln in lines)
print(
    "boots/reset lines",
    boots
    or f"0 (not visible in an API capture; {lost_sessions} lost API sessions, {gaps} reconnect failures)",
    "idf",
    sorted({re.search(r"idf=(\S+)", ln)[1] for _, ln in lines if "idf=" in ln}),
)
print_warnings(lines, 30, r"[0-9A-F]{2}(:[0-9A-F]{2}){5}|\(\d+\)|0x[0-9a-f]+|\d+")
try:
    p = [
        x.split()[1]
        for x in pathlib.Path(f"ping-{T}.txt").read_text(encoding="utf-8").splitlines()
    ]
    ok = [float(x) for x in p if x != "lost"]
    print(
        f"ping {len(ok)}/{len(p)} answered ({100 * len(ok) / max(len(p), 1):.0f}%), median {med(ok)} ms, p90 {p90(ok)} ms"
    )
except FileNotFoundError:
    print(f"no ping-{T}.txt, ping not measured")
if not steps:  # no start mark: nothing was run or the marks file is empty
    flagged.append(f"no steps in marks-{T}.txt")
if suite_incomplete is None and steps:
    flagged.append("suite not finished: no suite done mark after the last step")
elif suite_incomplete:
    flagged.append(f"suite incomplete: {suite_incomplete}")
flagged += [f"{s}: cut short, no end mark" for s, e in ended.items() if not e]
flagged += [
    f"{s}: short of its rounds or its driver failed in its last run"
    for s, v in step_result.items()
    if v != "ok"
]
if dead_bg:
    flagged.append(
        f"{' and '.join(sorted(dead_bg))} died during a run: its data has a hole"
    )
flagged += [f"{s}: last driver run rc={rc}" for s, rc in last_rc.items() if rc]
# a reset line in a serial capture means the proxy rebooted during the suite; reconnect failures in an API
# capture mean it was gone for a while, a lost API session that it restarted or dropped the link
if boots or gaps or lost_sessions:
    flagged.append(
        f"{boots} reset lines, {lost_sessions} lost API sessions, {gaps} API reconnect failures: the proxy "
        "restarted or went away"
    )
if flagged:
    print("FLAGGED:", "; ".join(flagged))
    sys.exit(1)
