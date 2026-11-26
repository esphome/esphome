"""summ.py NAME...: per build summary of cm-NAME-{1,2}.json (churnmix.py --json) and q-NAME.json (quiet run)."""

import json
import pathlib
import sys

for n in sys.argv[1:]:
    rows = []
    for r in (1, 2):
        f = f"cm-{n}-{r}.json"
        if not pathlib.Path(f).exists():
            continue
        d = json.loads(pathlib.Path(f).read_text(encoding="utf-8"))
        ch = d["churn"]
        fails = sum(v["connect_fail"] + v["no_state"] for v in ch.values()) + sum(
            v["no_200"] for k, v in ch.items() if k != "rst_mid"
        )
        rest_ok = sum(
            k2["ok"]
            for ph in d["rest"].values()
            for kk, k2 in ph.items()
            if kk != "final_switch"
        )
        rest_fail = sum(k2["fail"] for ph in d["rest"].values() for k2 in ph.values())
        rest_max = max(
            (k2["max_ms"] for ph in d["rest"].values() for k2 in ph.values()), default=0
        )
        obs = d["observers"]

        def hp(o, ph):
            v = [
                x[2] for x in o["heap"] if x[1] == ph and isinstance(x[2], (int, float))
            ]
            return (int(min(v)), int(v[-1])) if v else (None, None)

        o = obs[0]
        post = d["post"]
        rows.append(
            {
                "r": r,
                "cycles": d["cycles"],
                "secs": d["churn_seconds"],
                "fails": fails,
                "first": d["first_state_ms"],
                "rest_ok": rest_ok,
                "rest_fail": rest_fail,
                "rest_max": rest_max,
                "events": [x["events"] for x in obs],
                "missed": [x["missed"] for x in obs],
                "dup": [x["dup"] for x in obs],
                "reconn": [x["reconnects"] for x in obs],
                "relay": [x["relay_events"] for x in obs],
                "toggles": d["toggles_ok"],
                "final": [x.get("relay_final_ok") for x in obs],
                "reboots": [x.get("reboots") for x in obs],
                "heap_base": hp(o, "base"),
                "heap_churn": hp(o, "churn"),
                "heap_settle": hp(o, "settle"),
                "post": [
                    (p.get("status"), p.get("states"), p.get("first_ms"))
                    for _, p in sorted(post.items())
                ],
            }
        )
    q = None
    if pathlib.Path(f"q-{n}.json").exists():
        qd = json.loads(pathlib.Path(f"q-{n}.json").read_text(encoding="utf-8"))
        q = [(x["bare_counter"], x["missed"], x["reconnects"]) for x in qd["observers"]]
    print(f"== {n} quiet(counter,missed,reconn)={q}")
    for x in rows:
        print("  ", x)
