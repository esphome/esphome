"""Call reboot_during (a slow request in flight, another waiting, then App.safe_reboot) and time the reboot.
Usage: reboottest.py IP URL"""

import asyncio
import re
import sys
import time

import aioesphomeapi

IP, URL = sys.argv[1], sys.argv[2]


async def main():
    c = aioesphomeapi.APIClient(IP, 6053, None)
    await c.connect(login=True)
    _, svcs = await c.list_entities_services()
    s = {x.name: x for x in svcs}
    t0 = time.monotonic()

    def on_log(m):
        t = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
        if re.search(
            r"hrt|http_request|safe_mode|shutdown|Rebooting|reboot", t, re.IGNORECASE
        ):
            print(f"{time.monotonic() - t0:7.2f} {t[:150]}", flush=True)

    c.subscribe_logs(on_log, log_level=aioesphomeapi.LogLevel.LOG_LEVEL_DEBUG)
    await asyncio.sleep(1)
    t0 = time.monotonic()
    await c.execute_service(s["reboot_during"], {"url": URL})
    while time.monotonic() - t0 < 30:
        try:
            await asyncio.wait_for(c.device_info(), 1.0)
            await asyncio.sleep(0.2)
        except Exception:  # noqa: BLE001
            print(f"{time.monotonic() - t0:7.2f} API connection gone", flush=True)
            break
    for _ in range(60):
        try:
            c2 = aioesphomeapi.APIClient(IP, 6053, None)
            await asyncio.wait_for(c2.connect(login=True), 3)
            i = await c2.device_info()
            print(f"{time.monotonic() - t0:7.2f} back: {i.name}", flush=True)
            await c2.disconnect()
            return
        except Exception:  # noqa: BLE001
            await asyncio.sleep(1)


asyncio.run(main())
