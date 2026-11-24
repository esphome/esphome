"""Hold an API connection with log and state subscriptions for SECONDS, reconnecting after each drop.

usage: apihold.py HOST SECONDS
Prints each drop and the final counts of log lines, states, connections and drops.
"""

import asyncio
import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, stop_event  # noqa: E402


async def main() -> None:
    host, secs = sys.argv[1], float(sys.argv[2])
    end = time.monotonic() + secs
    n = {"logs": 0, "states": 0, "conns": 0, "drops": 0}

    def bump(key: str) -> None:
        n[key] += 1

    while time.monotonic() < end:
        ev, on_stop = stop_event()
        c = await connect(host, tries=120, on_stop=on_stop)
        n["conns"] += 1
        c.subscribe_logs(lambda _m: bump("logs"))
        c.subscribe_states(lambda _s: bump("states"))
        try:
            await asyncio.wait_for(ev.wait(), max(1, end - time.monotonic()))
            n["drops"] += 1
            print(time.strftime("%T"), "drop", n, flush=True)
        except TimeoutError:
            await c.disconnect()
    print("END", n, flush=True)


asyncio.run(main())
