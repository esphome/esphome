"""press.py HOST OBJECT_ID : press a button entity over the API (env API_KEY)"""

import asyncio
import contextlib
import os
import sys

from aioesphomeapi import APIClient


async def main(host, oid):
    c = APIClient(host, 6053, None, noise_psk=os.environ["API_KEY"])
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    key = next(e.key for e in ents if e.object_id == oid)
    c.button_command(key)
    await asyncio.sleep(1)
    with contextlib.suppress(Exception):
        await c.disconnect()
    print("pressed", oid)


asyncio.run(main(sys.argv[1], sys.argv[2]))
