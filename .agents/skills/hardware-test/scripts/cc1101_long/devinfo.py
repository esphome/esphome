"""devinfo.py HOST...: name, MAC, model and build of each device (API_KEY in the env for Noise)."""

import asyncio
import os
import sys

from aioesphomeapi import APIClient, APIConnectionError

KEY = os.environ.get("API_KEY") or None


async def one(h):
    c = APIClient(h, 6053, None, noise_psk=KEY)
    try:
        await asyncio.wait_for(c.connect(login=True), 8)
        i = await c.device_info()
        print(h, i.name, i.mac_address, i.model, i.compilation_time, i.esphome_version)
        await c.disconnect()
    except (OSError, APIConnectionError) as e:
        print(h, "ERR", type(e).__name__, e)


async def main():
    await asyncio.gather(*(one(h) for h in sys.argv[1:]))


asyncio.run(main())
