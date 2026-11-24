"""Reboot timing driver for on_client_synced (plaintext API on port 6053, a restart button named "Restart").

Usage: drive2.py HOST selfreboot D1,D2,...              a client subscribes to states and presses Restart D ms later
       drive2.py HOST hareboot TOKENFILE ENTRY D1,D2,...  reload the Home Assistant entry ENTRY, press Restart D ms later
env: HA_URL (default http://127.0.0.1:8129) for hareboot.
Waits for the device to come back after each reboot. Prints RUN lines with a wall clock time.
"""

import asyncio
import contextlib
import os
from pathlib import Path
import sys
import time
import urllib.request

from aioesphomeapi import APIClient, ButtonInfo

HOST = sys.argv[1]
MODE = sys.argv[2]
HA_URL = os.environ.get("HA_URL", "http://127.0.0.1:8129")


def ts() -> str:
    return time.strftime("%H:%M:%S") + f".{int(time.time() * 1000) % 1000:03d}"


async def mk(name: str, timeout: float = 15) -> APIClient:
    c = APIClient(HOST, 6053, None, client_info=name)
    await asyncio.wait_for(c.connect(login=True), timeout)
    return c


async def restart_key(c: APIClient) -> int:
    entities, _ = await c.list_entities_services()
    return next(
        e.key for e in entities if isinstance(e, ButtonInfo) and e.name == "Restart"
    )


async def close(c: APIClient) -> None:
    await asyncio.sleep(2)
    with contextlib.suppress(Exception):  # the device is rebooting
        await c.disconnect()


async def wait_back() -> None:
    await asyncio.sleep(3)
    for _ in range(60):
        try:
            c = await mk("hwtest-probe", 3)
            await c.disconnect()
            return
        except Exception:  # noqa: BLE001  still booting
            await asyncio.sleep(1)
    raise SystemExit("device did not come back")


async def selfreboot(d: int) -> None:
    probe = await mk("hwtest-keyprobe")
    key = await restart_key(probe)
    await probe.disconnect()
    await asyncio.sleep(1)
    c = await mk(f"hwtest-rb-{d:03d}")
    c.subscribe_states(lambda s: None)
    if d:
        await asyncio.sleep(d / 1000)
    print(f"RUN {ts()} selfreboot d={d} press", flush=True)
    c.button_command(key)
    await close(c)


async def hareboot(tok: str, entry: str, d: int) -> None:
    c = await mk("hwtest-press")
    key = await restart_key(c)
    await asyncio.sleep(1)
    req = urllib.request.Request(
        f"{HA_URL}/api/config/config_entries/entry/{entry}/reload",
        b"{}",
        {"Authorization": "Bearer " + tok, "Content-Type": "application/json"},
    )
    print(f"RUN {ts()} hareboot d={d} reload", flush=True)
    await asyncio.to_thread(lambda: urllib.request.urlopen(req, timeout=30).read())
    if d:
        await asyncio.sleep(d / 1000)
    print(f"RUN {ts()} hareboot d={d} press", flush=True)
    c.button_command(key)
    await close(c)


async def main() -> None:
    if MODE == "selfreboot":
        for d in (int(x) for x in sys.argv[3].split(",")):
            await selfreboot(d)
            await wait_back()
            await asyncio.sleep(8)  # let Home Assistant reconnect and sync
    elif MODE == "hareboot":
        tok = Path(sys.argv[3]).read_text(encoding="utf-8").strip()
        for d in (int(x) for x in sys.argv[5].split(",")):
            await hareboot(tok, sys.argv[4], d)
            await wait_back()
            await asyncio.sleep(8)
    print(f"RUN {ts()} done", flush=True)


asyncio.run(main())
