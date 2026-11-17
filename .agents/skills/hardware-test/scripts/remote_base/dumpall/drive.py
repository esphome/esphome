"""drive.py HOST: TX all twice at DEBUG, once at VERBOSE, then back to DEBUG."""

import asyncio
import pathlib
import sys

from aioesphomeapi import APIClient, APIConnectionError, ButtonInfo


async def main():
    c = APIClient(
        sys.argv[1],
        6053,
        None,
        noise_psk=pathlib.Path(".apikey").read_text(encoding="utf-8").strip(),
    )
    for _ in range(60):
        try:
            await asyncio.wait_for(c.connect(login=True), 10)
            break
        except (OSError, APIConnectionError):
            await asyncio.sleep(2)
    ents, _ = await c.list_entities_services()
    b = {e.object_id: e.key for e in ents if isinstance(e, ButtonInfo)}
    for step in ("tx_all", "tx_all", "verbose", "tx_all", "debug"):
        c.button_command(b[step])
        await asyncio.sleep(9 if step == "tx_all" else 1)
    await c.disconnect()
    print("driven")


asyncio.run(main())
