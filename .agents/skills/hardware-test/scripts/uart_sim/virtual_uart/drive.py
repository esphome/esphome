"""drive.py HOST SECONDS OUT: API log and state capture for the virtual_uart harness.

Writes OUT.log (every log line) and OUT.states (one line per state update) and prints a summary per entity.
"""

import asyncio
import pathlib
import sys
import time

from aioesphomeapi import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
from apiconn import connect, log_text  # noqa: E402


async def main() -> None:
    host, secs, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
    c = await connect(host)
    ents, _ = await c.list_entities_services()
    names = {e.key: e.name for e in ents}
    t0 = time.monotonic()
    counts: dict[str, int] = {}
    last: dict[str, float] = {}
    with (
        pathlib.Path(out + ".log").open("w", encoding="utf-8") as logf,
        pathlib.Path(out + ".states").open("w", encoding="utf-8") as stf,
    ):

        def on_log(msg) -> None:
            logf.write(f"{time.monotonic() - t0:8.2f} {log_text(msg)}\n")

        def on_state(s) -> None:
            n = names.get(s.key, str(s.key))
            v = getattr(s, "state", None)
            stf.write(f"{time.monotonic() - t0:8.2f} {n} = {v}\n")
            counts[n] = counts.get(n, 0) + 1
            last[n] = v

        c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
        c.subscribe_states(on_state)
        await asyncio.sleep(secs)
        await c.disconnect()
    for n in sorted(counts):
        print(f"{n}: {counts[n]} updates, last {last[n]}")


asyncio.run(main())
