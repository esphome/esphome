"""Normalise mosquitto_sub output (topic TAB payload) for one device: drop other devices, replace the name, drop dev block.

usage: mqtt_norm.py IN NAME OUT
"""

import json
import pathlib
import sys

IN, NAME, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
rows = {}
with pathlib.Path(IN).open(encoding="utf-8") as f:
    for line in f:
        topic, _, payload = line.rstrip("\n").partition("\t")
        if (
            f"/{NAME}/" not in topic and not topic.startswith(f"{NAME}/")
        ) or topic == f"{NAME}/debug":
            continue
        topic = topic.replace(NAME, "DEV")
        try:
            obj = json.loads(payload)
        except ValueError:
            rows[topic] = payload.replace(NAME, "DEV")
            continue
        if isinstance(obj, dict):
            obj.pop("dev", None)
            obj = {
                k: (v.replace(NAME, "DEV") if isinstance(v, str) else v)
                for k, v in obj.items()
            }
        rows[topic] = obj
with pathlib.Path(OUT).open("w", encoding="utf-8") as f:
    f.writelines(
        json.dumps({"topic": t, "payload": rows[t]}, sort_keys=True) + "\n"
        for t in sorted(rows)
    )
print(f"{len(rows)} topics for {NAME}")
