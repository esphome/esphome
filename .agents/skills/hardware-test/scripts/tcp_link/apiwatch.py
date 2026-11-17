"""apiwatch.py HOST PORT SECONDS [KEYFILE]: print every state change of a device's entities with a timestamp, starting
with the states sent on connect (missing_state shows as 'unknown'). Used to check the tcp link's `connected` binary
sensor reads OFF before the first connection, not unknown."""

import asyncio
import pathlib
import sys
import time

from aioesphomeapi import APIClient, APIConnectionError

HOST, PORT, SECS = sys.argv[1], int(sys.argv[2]), float(sys.argv[3])
KEY = (
    pathlib.Path(sys.argv[4] if len(sys.argv) > 4 else ".apikey")
    .read_text(encoding="utf-8")
    .strip()
)
T0 = time.monotonic()


async def main():
    c = APIClient(HOST, PORT, None, noise_psk=KEY)
    for _ in range(60):
        try:
            await asyncio.wait_for(c.connect(login=True), 5)
            break
        except (OSError, APIConnectionError):
            await asyncio.sleep(0.25)
    else:
        sys.exit("no API connection")
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
