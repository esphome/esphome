"""press.py HOST N: press the TX all button N times, 14 s apart."""

import asyncio
import pathlib
import sys

from aioesphomeapi import APIClient, APIConnectionError, ButtonInfo


async def main():
    key = pathlib.Path(".apikey").read_text(encoding="utf-8").strip()
    c = APIClient(sys.argv[1], 6053, None, noise_psk=key)
    for _ in range(60):
        try:
            await asyncio.wait_for(c.connect(login=True), 10)
            break
        except (OSError, APIConnectionError):
            await asyncio.sleep(2)
    ents, _ = await c.list_entities_services()
    b = next(e for e in ents if isinstance(e, ButtonInfo) and e.object_id == "tx_all")
    for _ in range(int(sys.argv[2])):
        c.button_command(b.key)
        await asyncio.sleep(14)
    await c.disconnect()
    print("pressed", sys.argv[2])


asyncio.run(main())
