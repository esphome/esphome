"""trcheck.py D1HOST D1VAR E32HOST E32VAR SECONDS OUT.json: watch the entity_transport remote entities on both
boards over the API and compare each received value with the sender's own published values."""

import asyncio
import json
import math
import pathlib
import struct
import sys
import time

from aioesphomeapi import APIClient, APIConnectionError

D1, D1V, E32, E32V, SECS, OUT = (
    sys.argv[1],
    sys.argv[2],
    sys.argv[3],
    sys.argv[4],
    float(sys.argv[5]),
    sys.argv[6],
)
KEY = pathlib.Path(".apikey").read_text(encoding="utf-8").strip()


def f32(v):
    return struct.unpack("<f", struct.pack("<f", v))[0]


def expect(t):  # 12345.678f + t * 0.001f - (t % 7) * 1000.5f in single precision
    return f32(f32(f32(12345.678) + f32(f32(t) * f32(0.001))) - f32((t % 7) * 1000.5))


async def conn(host):
    c = APIClient(host, 6053, None, noise_psk=KEY)
    for _ in range(30):
        try:
            await asyncio.wait_for(c.connect(login=True), 10)
            return c
        except (OSError, APIConnectionError):
            await asyncio.sleep(1)
    sys.exit(f"no connection to {host}")


async def main():
    boards = {"d1": (D1, D1V), "e32": (E32, E32V)}
    hist = {
        b: {"count": [], "float": set(), "bin": [], "count_to_float": {}}
        for b in boards
    }
    obs = {}  # (receiver, kind, field) -> list of (t, value, missing)
    clients = {}
    for b, (host, _) in boards.items():
        c = await conn(host)
        clients[b] = c
        ents, _ = await c.list_entities_services()
        keys = {e.key: e.object_id for e in ents}
        peer_var = boards["e32" if b == "d1" else "d1"][1]
        last = {}

        def cb(s, b=b, keys=keys, peer_var=peer_var, last=last):
            oid = keys.get(s.key)
            t = time.monotonic()
            if oid is None:
                return
            miss = bool(s.missing_state) or (
                isinstance(s.state, float) and math.isnan(s.state)
            )
            h = hist[b]
            if oid == "local_count" and not miss:
                h["count"].append((t, int(s.state)))
                last["count"] = int(s.state)
            elif oid == "local_float" and not miss:
                h["float"].add(f32(s.state))
                if "count" in last:
                    h["count_to_float"][last["count"]] = f32(s.state)
            elif oid == "local_bin":
                h["bin"].append((t, bool(s.state)))
            elif oid.startswith("r_"):
                _, kind, var, field = oid.split("_")
                if var == peer_var:
                    obs.setdefault(f"{b}<-{kind}:{field}", []).append(
                        (t, s.state, miss)
                    )
                elif not miss:
                    obs.setdefault(f"{b}<-{kind}:{field}:otherpeer", []).append(
                        (t, s.state, False)
                    )

        c.subscribe_states(cb)
    await asyncio.sleep(SECS)
    for c in clients.values():
        await c.disconnect()
    res = {"d1": D1V, "e32": E32V, "seconds": SECS, "links": {}}
    for key, lst in sorted(obs.items()):
        recv, rest = key.split("<-")
        kind, field = rest.split(":")[:2]
        sender = "e32" if recv == "d1" else "d1"
        h = hist[sender]
        if key.endswith("otherpeer"):
            res["links"][key] = {"values_after_initial": len(lst) - 1}
            continue
        vals = [(t, v) for t, v, m in lst if not m][
            1:
        ]  # the first is the state from before the run
        missing_after_first = sum(
            1
            for i, (t, v, m) in enumerate(lst)
            if m and any(not mm for _, _, mm in lst[:i])
        )
        r = {"updates": len(vals), "missing_after_first": missing_after_first}
        if field == "count":
            sent = {c for _, c in h["count"]}
            first = min(sent) if sent else 0
            r["before_run"] = sum(1 for _, v in vals if int(v) < first)
            last_sent = max(sent) if sent else 0
            r["not_sent"] = sum(
                1
                for _, v in vals
                if int(v) < first - 0
                and False
                or int(v) > last_sent + 60
                or int(v) != v
            )
            r["api_gap_values"] = sum(
                1
                for _, v in vals
                if first <= int(v) <= last_sent and int(v) not in sent
            )
            lags = []
            for t, v in vals:
                prior = [c for tt, c in h["count"] if tt <= t]
                if prior:
                    lags.append(prior[-1] - int(v))
            r["max_lag_s"] = max(lags) if lags else None
        elif field == "flt":
            ok_t = [
                t
                for t, v, _m in obs[key.replace("flt", "count")]
                if not (isinstance(v, float) and math.isnan(v))
                and int(v) >= min(c for _, c in h["count"])
            ]
            t0 = min(ok_t) if ok_t else 0
            lo, hi = min(c for _, c in h["count"]), max(c for _, c in h["count"]) + 60
            allowed = {expect(c) for c in range(max(1, lo - 30), hi + 1)}
            r["not_sent"] = sum(1 for t, v in vals if t >= t0 and f32(v) not in allowed)
            r["sender_float_mismatch"] = sum(
                1 for c, fv in h["count_to_float"].items() if fv != expect(c)
            )
        else:
            sent = [v for _, v in h["bin"]]
            r["transitions"] = sum(
                1 for i in range(1, len(vals)) if vals[i][1] != vals[i - 1][1]
            )
            r["sender_transitions"] = sum(
                1 for i in range(1, len(sent)) if sent[i] != sent[i - 1]
            )
        res["links"][key] = r
    ok = True
    for k, r in res["links"].items():
        if k.endswith("otherpeer"):
            ok = ok and r["values_after_initial"] == 0
            continue
        if (
            r["updates"] < 3
            or r.get("not_sent", 0)
            or r.get("sender_float_mismatch", 0)
        ):
            ok = False
        if "transitions" in r and r["transitions"] < 2:
            ok = False
    res["expected_links_present"] = (
        len([k for k in res["links"] if not k.endswith("otherpeer")]) == 12
    )
    res["pass"] = ok and res["expected_links_present"]
    print(json.dumps(res, indent=1))
    pathlib.Path(OUT).write_text(json.dumps(res, indent=1), encoding="utf-8")


asyncio.run(main())
