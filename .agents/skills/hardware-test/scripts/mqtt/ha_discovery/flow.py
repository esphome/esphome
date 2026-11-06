"""Add the MQTT integration to Home Assistant through the config flow.

usage: flow.py TOKENFILE   env: HA_URL (default http://127.0.0.1:8129), HA_BROKER (default 127.0.0.1),
HA_BROKER_PORT (default 1883). The protocol and other_settings fields are required by the current flow.
"""

import json
import os
import pathlib
import sys
import urllib.error
import urllib.request

BASE = os.environ.get("HA_URL", "http://127.0.0.1:8129")
TOK = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").strip()


def req(path: str, data: dict | None = None) -> dict:
    h = {"Authorization": "Bearer " + TOK, "Content-Type": "application/json"}
    body = json.dumps(data).encode() if data is not None else None
    try:
        with urllib.request.urlopen(
            urllib.request.Request(BASE + path, body, h), timeout=30
        ) as r:
            return json.loads(r.read() or b"null")
    except urllib.error.HTTPError as e:
        sys.exit(f"HTTP {e.code} {e.read()[:500]!r}")


flow = req("/api/config/config_entries/flow", {"handler": "mqtt"})
if flow.get("type") == "menu":
    flow = req(
        f"/api/config/config_entries/flow/{flow['flow_id']}", {"next_step_id": "broker"}
    )
print([(f["name"], f.get("required")) for f in flow.get("data_schema", [])])
res = req(
    f"/api/config/config_entries/flow/{flow['flow_id']}",
    {
        "broker": os.environ.get("HA_BROKER", "127.0.0.1"),
        "port": int(os.environ.get("HA_BROKER_PORT", "1883")),
        "protocol": "3.1.1",
        "other_settings": {
            "set_client_cert": False,
            "set_ca_cert": "off",
            "transport": "tcp",
        },
    },
)
print(res.get("type"), res.get("title"), res.get("errors"), res.get("step_id"))
if res.get("type") != "create_entry":
    sys.exit(1)
