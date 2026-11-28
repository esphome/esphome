"""drive.py HOST LOGFILE RUNS [GAP]: press the Run button RUNS times, GAP seconds apart (default 25).

Writes every API log line to LOGFILE and prints the harness lines plus warnings and errors. API_KEY as in apiconn.py.
"""

import asyncio
import pathlib
import sys
import time

from aioesphomeapi import ButtonInfo, LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text  # noqa: E402

HOST, LOGF, RUNS = sys.argv[1], sys.argv[2], int(sys.argv[3])
GAP = float(sys.argv[4]) if len(sys.argv) > 4 else 25


async def main() -> None:
    c = await connect(HOST)
    di = await c.device_info()
    print("device", di.name, di.esphome_version, di.compilation_time, flush=True)
    ents, _ = await c.list_entities_services()
    run = next(e for e in ents if isinstance(e, ButtonInfo) and e.name == "Run")
    t0 = time.time()
    with pathlib.Path(LOGF).open("w", encoding="utf-8") as log:

        def on_log(m) -> None:
            line = log_text(m)
            log.write(f"{time.time() - t0:8.3f} {line}\n")
            if "[lfh" in line or line.startswith(("[W]", "[E]")):
                print(f"{time.time() - t0:8.3f} {line}", flush=True)

        c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_VERBOSE)
        await asyncio.sleep(3)
        for i in range(RUNS):
            print(f"--- run {i + 1}", flush=True)
            c.button_command(run.key)
            await asyncio.sleep(GAP)
        await c.disconnect()
        await asyncio.sleep(0.5)


asyncio.run(main())
