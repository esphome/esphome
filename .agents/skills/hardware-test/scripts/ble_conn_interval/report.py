"""report.py TAG CAPTURE: summary of every TAG-*.jsonl scenario; HCI refusals counted from the serial capture in each
scenario's time window."""

import json
import pathlib
import re
import statistics
import sys

from stats import hms, quantile

tag, cap = sys.argv[1], sys.argv[2]
lines = pathlib.Path(cap).read_text(encoding="utf-8", errors="replace").splitlines()


def window(t0: float, t1: float) -> list[str]:
    a, b = hms(t0), hms(t1)
    return [ln for ln in lines if ln.startswith("[") and a <= ln[1:9] <= b]


def spread(ts: list[float]) -> str:
    if not ts:
        return "none resolved"
    return (
        f"min={min(ts):.2f} med={statistics.median(ts):.2f} p95={quantile(ts, 0.95):.2f} max={max(ts):.2f} "
        f"over8={sum(t > 8 for t in ts)}"
    )


for path in sorted(pathlib.Path().glob(f"{tag}-*.jsonl")):
    rows = [json.loads(ln) for ln in path.read_text(encoding="utf-8").splitlines()]
    if not rows:
        continue
    w = window(rows[0].get("start", rows[0]["ts"] - 30) - 5, rows[-1]["ts"] + 5)
    rej = sum("Rejected: Resources" in ln for ln in w)
    closed = sum("Remote closed during discovery" in ln for ln in w)
    ivs = sorted(
        {
            m[1]
            for ln in w
            if (m := re.search(r"IVLOG connect \S+ if=3 interval=(\d+)", ln))
        }
    )
    upd = sorted(
        {
            (m[1], m[2])
            for ln in w
            if (m := re.search(r"IVLOG update \S+ status=(\d+) interval=(\d+)", ln))
        }
    )
    name = path.name[len(tag) + 1 : -6]
    if name.startswith("third"):
        # rounds where the two held links never came up did not try the third link
        hf = sum(r.get("result") == "held_failed" for r in rows)
        rows = [r for r in rows if r.get("result") != "held_failed"]
        ok = sum(r.get("connected") is True for r in rows)
        to = sum("Timeout" in (r.get("exc") or "") for r in rows)
        f133 = sum(r.get("err") == 133 for r in rows)
        tc = [r["t_conn"] for r in rows if r.get("connected")]
        print(
            f"{name}: rounds={len(rows)} held_failed={hf} connected={ok} err133={f133} api_timeouts={to} HCI_0x0D_lines={rej} "
            f"t_conn_med={statistics.median(tc) if tc else None} conn_iv={ivs} updates={upd}"
        )
    elif any("detail" in r for r in rows):
        rr = []
        for r in rows:
            # attempts that never reached the proxy (API link down) are not measurements
            detail = [
                a for a in r.get("detail", []) if a.get("exc") != "clear cache failed"
            ]
            if detail:
                rr.append({**r, "detail": detail})
        att = [len(r["detail"]) for r in rr]
        ts = [a["t_svc"] for r in rr for a in r["detail"] if a.get("t_svc")]
        tcmd = [a["t_cmd"] for r in rr for a in r["detail"] if a.get("t_cmd")]
        sizes = sorted(
            {
                (a.get("n_svc"), a.get("n_chr"), a.get("n_dsc"))
                for r in rr
                for a in r["detail"]
                if a.get("t_svc")
            }
        )
        print(
            f"{name}: rounds={len(rr)} attempts={sum(att)} per_round={att} never_in_5={sum(not r['ok'] for r in rr)} "
            f"resolved_attempts={len(ts)} t_svc[{spread(ts)}] cmd_ok={len(tcmd)} sizes={sizes} "
            f"remote_closed={closed} HCI_0x0D={rej} conn_iv={ivs} updates={upd}"
        )
    else:
        rows = [
            r
            for r in rows
            if r.get("exc") != "clear cache failed"
            and r.get("result") != "eve_not_seen"
        ]
        ts = [r["t_svc"] for r in rows if r.get("t_svc")]
        sizes = sorted(
            {
                (r.get("n_svc"), r.get("n_chr"), r.get("n_dsc"))
                for r in rows
                if r.get("t_svc")
            }
        )
        print(
            f"{name}: rounds={len(rows)} resolved={len(ts)} t_svc[{spread(ts)}] sizes={sizes} "
            f"errs={[r.get('err') for r in rows]} remote_closed_during_discovery={closed} HCI_0x0D={rej} "
            f"conn_iv={ivs} updates={upd}"
        )
