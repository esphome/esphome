"""Read the device's time, date and datetime entities in Home Assistant, set each one and read again.

usage: hadrive.py TOKENFILE NAME   env: HA_URL (default http://127.0.0.1:8129)
NAME is a part of the entity ids to match. Exits 1 when an entity is missing, a set_value call failed or an entity
did not read back the value that was set.
"""

import datetime as dt
import pathlib
import sys
import time

from ha_rest import failed, req, req_or_exit

TOK = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").strip()
NAME = sys.argv[2]


def ents() -> dict[str, dict]:
    states = req_or_exit("/api/states", token=TOK)
    return {
        s["entity_id"]: s
        for s in states
        if s["entity_id"].split(".")[0] in ("time", "date", "datetime")
        and NAME in s["entity_id"]
    }


def show(tag: str) -> dict[str, dict]:
    e = ents()
    for eid, s in sorted(e.items()):
        print(f"{tag} {eid} = {s['state']!r}", flush=True)
    return e


def same(domain: str, state: str, value: str) -> bool:
    if domain != "datetime":
        return state == value
    try:
        return dt.datetime.fromisoformat(state) == dt.datetime.fromisoformat(value)
    except ValueError:
        return False


e = show("INITIAL")
fail = False
sets: dict[str, tuple[str, str]] = {}
for domain, field, value in (
    ("time", "time", "10:10:00"),
    ("date", "date", "2026-12-24"),
    ("datetime", "datetime", "2026-01-10T08:15:00+00:00"),
):
    eid = next((k for k in e if k.startswith(domain + ".")), None)
    if eid is None:
        print(f"MISSING {domain} entity")
        fail = True
        continue
    r = req(
        f"/api/services/{domain}/set_value",
        {"entity_id": eid, field: value},
        token=TOK,
    )
    bad = failed(r)
    fail |= bad
    print(f"SET {domain} {value} -> {'error ' + str(r) if bad else 'ok'}", flush=True)
    sets[eid] = (domain, value)
    time.sleep(2)
after = show("AFTER")
for eid, (domain, value) in sets.items():
    if not same(domain, state := after.get(eid, {}).get("state", ""), value):
        print(f"MISMATCH {eid}: set {value!r}, reads {state!r}")
        fail = True
print("FAIL" if fail else "PASS")
sys.exit(1 if fail else 0)
