"""First listing event of each entity in a web_server /events capture, values masked, in arrival order.
Usage: evnorm.py EVENTS_TXT   (diff the output of two builds)
Reads web.sh's curl capture (curl removes the chunk framing); entity_icons/web_dump.py keeps only state events."""

import json
import pathlib
import sys

seen, out = set(), []
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as f:
    for line in f:
        if not line.startswith("data: {"):
            continue
        try:
            d = json.loads(line[6:])
        except json.JSONDecodeError:  # curl's --max-time can cut the last event
            continue
        if "domain" not in d or d["id"] in seen:
            continue
        seen.add(d["id"])
        for k in ("value", "state", "uptime", "brightness", "speed_level"):
            d.pop(k, None)
        out.append(json.dumps(d, sort_keys=True))
if not out:
    sys.exit(f"no listing events in {sys.argv[1]}")
print("\n".join(out))
