"""apiwatch.py HOST PORT SECONDS [LOGFILE] (API_KEY in the env): print every state change of a device's entities with a
timestamp, starting with the states sent on connect (missing_state shows as 'unknown'). Used to check the tcp link's
`connected` binary sensor reads OFF before the first connection, not unknown. With LOGFILE, the device's log lines
(INFO and above, no colours) go there too. Exits 1 when the API connection drops during the watch."""

import asyncio
import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from aioesphomeapi import LogLevel  # noqa: E402
from apiconn import connect, log_text  # noqa: E402

HOST, PORT, SECS = sys.argv[1], int(sys.argv[2]), float(sys.argv[3])
LOGFILE = sys.argv[4] if len(sys.argv) > 4 else None
T0 = time.monotonic()


async def main():
    lost = asyncio.Event()

    async def on_stop(expected: bool) -> None:
        if not expected:
            print(f"{time.monotonic() - T0:8.3f} API connection lost", flush=True)
            lost.set()

    c = await connect(HOST, tries=60, timeout=5, port=PORT, on_stop=on_stop)
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
    if LOGFILE:
        logs = pathlib.Path(LOGFILE).open("w", encoding="utf-8", buffering=1)  # noqa: SIM115
        c.subscribe_logs(
            lambda m: logs.write(log_text(m) + "\n"), log_level=LogLevel.LOG_LEVEL_INFO
        )
    await asyncio.sleep(SECS)
    if lost.is_set():
        sys.exit("FAIL the API connection dropped")
    await c.disconnect()


asyncio.run(main())
