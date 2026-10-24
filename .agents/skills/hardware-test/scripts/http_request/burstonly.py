"""Run the burst action once and print each queued request as it completes.
Usage: burstonly.py IP URL N"""

import asyncio
import re
import sys
import time

import aioesphomeapi

IP, URL, N = sys.argv[1], sys.argv[2], int(sys.argv[3])


async def main():
    c = aioesphomeapi.APIClient(IP, 6053, None)
    await c.connect(login=True)
    _, svcs = await c.list_entities_services()
    s = {x.name: x for x in svcs}
    t0 = time.monotonic()
    done = []
    errors = []

    def on_log(m):
        t = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
        if "BURSTRESP" in t:
            done.append(t)
        elif "BURSTERR" in t:
            errors.append(t)
        if "hrt" in t or "Too many" in t:
            print(f"{time.monotonic() - t0:6.2f} {t[:120]}", flush=True)

    c.subscribe_logs(on_log, log_level=aioesphomeapi.LogLevel.LOG_LEVEL_INFO)
    await asyncio.sleep(1)
    t0 = time.monotonic()
    await c.execute_service(s["burst"], {"url": URL, "count": N})
    await asyncio.sleep(14)
    await c.disconnect()
    print(
        f"{len(done)} of {N} burst requests answered, {len(errors)} failed", flush=True
    )
    return 0 if len(done) >= N else 1


sys.exit(asyncio.run(main()))
