"""apiwatch.py HOST PORT SECONDS (API_KEY in the env): print every state change of a device's entities with a timestamp, starting
with the states sent on connect (missing_state shows as 'unknown'). Used to check the tcp link's `connected` binary
sensor reads OFF before the first connection, not unknown."""

import asyncio
import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect  # noqa: E402

HOST, PORT, SECS = sys.argv[1], int(sys.argv[2]), float(sys.argv[3])
T0 = time.monotonic()


async def main():
    c = await connect(HOST, tries=60, timeout=5, port=PORT)
    ents, _ = await c.list_entities_services()
    names = {e.key: e.object_id for e in ents}
    last = {}

    def cb(s):
        oid = names.get(s.key, s.key)
        v = "unknown" if getattr(s, "missing_state", False) else s.state
        if last.get(oid) != v:
            last[oid] = v
            print(f"{time.monotonic() - T0:8.3f} {oid} = {v}", flush=True)

    c.subscribe_states(cb)
    await asyncio.sleep(SECS)
    await c.disconnect()


asyncio.run(main())
