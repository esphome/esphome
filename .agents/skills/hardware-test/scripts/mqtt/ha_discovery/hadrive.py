"""Read the device's time, date and datetime entities in Home Assistant, set each one and read again.

usage: hadrive.py TOKENFILE NAME   env: HA_URL (default http://127.0.0.1:8129)
NAME is a part of the entity ids to match. Exits 1 when an entity is missing or a set_value call failed.
"""

import json
import os
import pathlib
import sys
import time
import urllib.error
import urllib.request

BASE = os.environ.get("HA_URL", "http://127.0.0.1:8129")
TOK = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").strip()
NAME = sys.argv[2]


def req(path: str, data: dict | None = None) -> dict | list:
    h = {"Authorization": "Bearer " + TOK, "Content-Type": "application/json"}
    body = json.dumps(data).encode() if data is not None else None
    try:
        with urllib.request.urlopen(
            urllib.request.Request(BASE + path, body, h), timeout=30
        ) as r:
            return json.loads(r.read() or b"null")
    except urllib.error.HTTPError as e:
        return {"http_error": e.code, "body": e.read().decode()[:300]}


def ents() -> dict[str, dict]:
    states = req("/api/states")
    if isinstance(states, dict) and "http_error" in states:
        sys.exit(f"/api/states failed: HTTP {states['http_error']} {states['body']}")
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


e = show("INITIAL")
fail = False
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
    r = req(f"/api/services/{domain}/set_value", {"entity_id": eid, field: value})
    bad = isinstance(r, dict) and "http_error" in r
    fail |= bad
    print(f"SET {domain} {value} -> {'error ' + str(r) if bad else 'ok'}", flush=True)
    time.sleep(2)
show("AFTER")
sys.exit(1 if fail else 0)
