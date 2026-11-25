"""soak.py HOST SECONDS OUT_PREFIX: count state updates per entity and keep the W and E log lines for SECONDS.

Reconnects after a drop. Writes OUT_PREFIX.json (counts, connects, levels, device info) and OUT_PREFIX-log.txt (every
log line). Use it on a real sensor before and after a build swap. API_KEY as in apiconn.py.
"""

import asyncio
from collections import Counter
import contextlib
import json
import pathlib
import re
import sys
import time

from aioesphomeapi import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text, stop_event  # noqa: E402

HOST, SECS, OUT = sys.argv[1], float(sys.argv[2]), sys.argv[3]
LEVEL = re.compile(r"\[([EWIDCV]+)\]\[")


async def main() -> None:
    counts: Counter = Counter()
    levels: Counter = Counter()
    warn: list[str] = []
    info: dict = {}
    connects = 0
    t_end = time.time() + SECS
    with pathlib.Path(OUT + "-log.txt").open("w", encoding="utf-8") as log:
        while time.time() < t_end:
            stopped, on_stop = stop_event()
            c = await connect(
                HOST, tries=max(1, int(t_end - time.time())), on_stop=on_stop
            )
            connects += 1
            di = await c.device_info()
            info = {
                "name": di.name,
                "esphome": di.esphome_version,
                "compiled": di.compilation_time,
            }
            ents, _ = await c.list_entities_services()
            names = {e.key: f"{type(e).__name__[:-4]}:{e.name}" for e in ents}

            def on_state(s, names: dict = names) -> None:
                counts[names.get(s.key, str(s.key))] += 1

            def on_log(m) -> None:
                line = log_text(m)
                stamp = time.strftime("%H:%M:%S")
                log.write(f"{stamp} {line}\n")
                if mm := LEVEL.match(line):
                    levels[mm.group(1)] += 1
                    if mm.group(1) in ("E", "W"):
                        warn.append(f"{stamp} {line}")

            c.subscribe_states(on_state)
            c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
            with contextlib.suppress(TimeoutError):
                await asyncio.wait_for(stopped.wait(), max(0.0, t_end - time.time()))
            with contextlib.suppress(Exception):
                await c.disconnect()
    res = {"info": info, "connects": connects, "levels": dict(levels), "warnings": warn}
    res["counts"] = dict(sorted(counts.items()))
    pathlib.Path(OUT + ".json").write_text(json.dumps(res, indent=1), encoding="utf-8")
    print(
        json.dumps({k: res[k] for k in ("info", "connects", "levels")}),
        "warnings",
        len(warn),
    )


asyncio.run(main())
