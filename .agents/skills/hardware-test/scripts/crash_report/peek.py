"""Print the crash report capability and whether the log replay shows a crash; optionally press a button instead.

usage: peek.py HOST [BUTTON_OBJECT_ID]   env: API_KEY (unset for a plaintext API)
"""

import asyncio
import os
import socket
import sys

from aioesphomeapi import APIClient
from aioesphomeapi.model import LogLevel


async def main() -> None:
    for _ in range(60):
        try:
            c = APIClient(
                socket.gethostbyname(sys.argv[1]),
                6053,
                None,
                noise_psk=os.environ.get("API_KEY"),
            )
            await asyncio.wait_for(c.connect(login=True), 10)
            break
        except Exception:  # noqa: BLE001
            await asyncio.sleep(1)
    else:
        sys.exit("no API connection")
    if len(sys.argv) > 2:
        ents, _ = await c.list_entities_services()
        c.button_command(next(e.key for e in ents if e.object_id == sys.argv[2]))
        print("pressed", sys.argv[2])
        await asyncio.sleep(1)
        return
    caps = await c.device_capabilities()
    lines: list[str] = []
    c.subscribe_logs(
        lambda m: lines.append(m.message.decode(errors="replace")),
        log_level=LogLevel.LOG_LEVEL_DEBUG,
        dump_config=True,
    )
    await asyncio.sleep(4)
    print(
        "caps",
        caps.crash_report,
        "replay lines",
        sum("CRASH DETECTED" in line for line in lines),
    )
    await c.disconnect()


asyncio.run(main())
