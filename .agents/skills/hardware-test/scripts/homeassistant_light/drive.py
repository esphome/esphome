"""Press a template button on the test device over the API, or list its buttons.

usage: drive.py HOST KEY list | press "Button name" [...]
"""

import asyncio
import sys

from aioesphomeapi import APIClient, ButtonInfo

HOST, KEY = sys.argv[1], sys.argv[2]
CMD, ARGS = sys.argv[3], sys.argv[4:]


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=KEY)
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    buttons = {e.name: e for e in ents if isinstance(e, ButtonInfo)}
    if CMD == "list":
        print("\n".join(sorted(buttons)))
    else:
        for name in ARGS:
            c.button_command(buttons[name].key)
            print("pressed", name)
            await asyncio.sleep(0.3)
    await asyncio.sleep(0.5)
    await c.disconnect()


asyncio.run(main())
