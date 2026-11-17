import asyncio
import pathlib
import sys

from aioesphomeapi import APIClient, APIConnectionError

KEY = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8").strip()


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
    await asyncio.gather(*(one(h) for h in sys.argv[2:]))


asyncio.run(main())
