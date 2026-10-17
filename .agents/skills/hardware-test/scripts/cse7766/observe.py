"""Subscribe to states and logs of a device over the API for N seconds and summarize its float sensors.

usage: observe.py IP SECONDS OUTFILE [NAME_FILTER]

Every float sensor state whose entity name contains NAME_FILTER (default: all) is appended to OUTFILE
with a relative timestamp, log lines are appended with a LOG prefix, and a one-line summary per sensor
(count, min, max, last) plus the warning and error count is printed at the end.
"""

import asyncio
import pathlib
import re
import sys
import time

from aioesphomeapi import APIClient, LogLevel

IP, SECS, OUT = sys.argv[1], float(sys.argv[2]), sys.argv[3]
FILTER = sys.argv[4] if len(sys.argv) > 4 else ""


async def main() -> None:
    c = APIClient(IP, 6053, None)
    await c.connect(login=True)
    di = await c.device_info()
    ents, _ = await c.list_entities_services()
    names = {e.key: e.name for e in ents}
    readings: dict[str, list[float]] = {}
    warnings: list[str] = []
    t0 = time.monotonic()
    with pathlib.Path(OUT).open("w", encoding="utf-8") as out:
        out.write(
            f"device {di.name} version {di.esphome_version} compiled {di.compilation_time}\n"
        )

        def on_state(st) -> None:
            n = names.get(st.key, str(st.key))
            if FILTER in n and hasattr(st, "state") and isinstance(st.state, float):
                readings.setdefault(n, []).append(st.state)
                out.write(f"[{time.monotonic() - t0:6.1f}] {n}: {st.state}\n")
                out.flush()

        def on_log(msg) -> None:
            line = (
                msg.message.decode(errors="replace")
                if isinstance(msg.message, bytes)
                else msg.message
            )
            line = re.sub(r"\x1b\[[0-9;]*m", "", line)
            out.write(f"[{time.monotonic() - t0:6.1f}] LOG {line}\n")
            out.flush()
            if "[W]" in line or "[E]" in line or "Invalid" in line:
                warnings.append(line)

        c.subscribe_states(on_state)
        c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_VERBOSE)
        await asyncio.sleep(SECS)
        await c.disconnect()
    summary = "; ".join(
        f"{k.split()[-1]} n={len(v)} min={min(v):.3f} max={max(v):.3f} last={v[-1]:.3f}"
        for k, v in sorted(readings.items())
    )
    print(f"{di.compilation_time}: {summary}; warnings/errors {len(warnings)}")
    for w in warnings[:5]:
        print("  ", w)


asyncio.run(main())
