"""Trigger ota.http_request.flash over the API, optionally during a burst of action requests.
Usage: otadrive.py IP FWURL MD5 BASE [burst]"""

import asyncio
import contextlib
import re
import sys
import time

import aioesphomeapi

IP, FW, SUM, BASE = sys.argv[1:5]
BURST = len(sys.argv) > 5


async def main():
    c = aioesphomeapi.APIClient(IP, 6053, None)
    await c.connect(login=True)
    i = await c.device_info()
    print("before:", i.name, i.compilation_time, flush=True)
    _, svcs = await c.list_entities_services()
    s = {x.name: x for x in svcs}
    t0 = time.monotonic()
    seen: list[str] = []

    def on_log(m):
        t = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
        seen.append(t)
        if re.search(r"hrt|http_request|ota|OTA|Too many", t):
            print(f"{time.monotonic() - t0:7.2f} {t[:170]}", flush=True)

    c.subscribe_logs(on_log, log_level=aioesphomeapi.LogLevel.LOG_LEVEL_INFO)
    await asyncio.sleep(1)
    if BURST:
        await c.execute_service(s["burst"], {"url": BASE + "/slow?d=1", "count": 6})
        await asyncio.sleep(0.5)
    await c.execute_service(s["otaflash"], {"url": FW, "sum": SUM})
    await asyncio.sleep(45)
    with contextlib.suppress(Exception):
        await c.disconnect()
    for _ in range(30):
        try:
            c2 = aioesphomeapi.APIClient(IP, 6053, None)
            await c2.connect(login=True)
            i = await c2.device_info()
            print("after:", i.name, i.compilation_time, flush=True)
            await c2.disconnect()
            break
        except Exception:  # noqa: BLE001
            await asyncio.sleep(2)
    else:
        print("RESULT FAIL: not reachable after the flash", flush=True)
        return 1
    if any("OTAFLASH returned" in t for t in seen):
        print("RESULT FAIL: the flash action returned, the update failed", flush=True)
        return 1
    if not any(re.search(r"(?i)update complete|rebooting|successful", t) for t in seen):
        print("RESULT FAIL: no update complete line before the reboot", flush=True)
        return 1
    print("RESULT PASS", flush=True)
    return 0


sys.exit(asyncio.run(main()))
