"""Add a host build's API to a throwaway Home Assistant and allow it to call actions.

usage: haflow.py TOKENFILE   env: HA_URL (default http://127.0.0.1:8129)
Runs the esphome config flow against 127.0.0.1:6053, then turns on allow_service_calls in the options flow.
"""

import pathlib
import sys

sys.path.insert(
    0, str(pathlib.Path(__file__).resolve().parent.parent / "mqtt" / "ha_discovery")
)
from ha_rest import req_or_exit  # noqa: E402

tok = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").strip()
flow = "/api/config/config_entries/flow"
f = req_or_exit(flow, {"handler": "esphome"}, token=tok)
r = req_or_exit(
    f"{flow}/{f['flow_id']}", {"host": "127.0.0.1", "port": 6053}, token=tok
)
if r.get("type") == "form" and r.get("errors"):
    sys.exit(f"config flow step {r.get('step_id')} failed: {r['errors']}")
if r.get("type") == "form":  # confirmation step
    r = req_or_exit(f"{flow}/{r['flow_id']}", {}, token=tok)
if r.get("type") != "create_entry":
    sys.exit(
        f"config flow ended with {r.get('type')} {r.get('errors') or r.get('reason')}"
    )
opts = "/api/config/config_entries/options/flow"
o = req_or_exit(opts, {"handler": r["result"]["entry_id"]}, token=tok)
o = req_or_exit(f"{opts}/{o['flow_id']}", {"allow_service_calls": True}, token=tok)
if o.get("type") != "create_entry":
    sys.exit(
        f"options flow ended with {o.get('type')} {o.get('errors') or o.get('reason')}"
    )
print("added", r.get("title"), "with allow_service_calls")
