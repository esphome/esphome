"""drive.py HOST ROUNDS [soak]: press each tx button ROUNDS times in a fixed order, then optionally soak."""

import asyncio
import sys

from aioesphomeapi import APIClient, BinarySensorInfo, ButtonInfo

HOST, ROUNDS = sys.argv[1], int(sys.argv[2])
ORDER = [
    "tx nec",
    "tx raw21a",
    "tx raw21b",
    "tx raw41",
    "tx raw43",
    "tx raw121",
    "tx raw245",
]


async def main():
    c = APIClient(HOST, 6053, None)
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    b = {e.name: e for e in ents if isinstance(e, ButtonInfo)}
    bs = {e.key: e.name for e in ents if isinstance(e, BinarySensorInfo)}
    states = []
    c.subscribe_states(lambda s: states.append(s) if s.key in bs else None)
    await asyncio.sleep(2)
    for _r in range(ROUNDS):
        for n in ORDER:
            c.button_command(b[n].key)
            await asyncio.sleep(1.5)
    if len(sys.argv) > 3:
        c.button_command(b["soak"].key)
        await asyncio.sleep(90)
    on = sum(1 for s in states if s.state)
    print(f"binary sensor api states: {len(states)} total, {on} on")
    await c.disconnect()


asyncio.run(main())
