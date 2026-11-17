"""press.py HOST BUTTON[:SECS]...: press each button (by object id) in order, waiting SECS (default 1) after each.

hostharness: `press.py HOST tx_all:14 tx_all:14 ...`; dumpall: `press.py HOST tx_all:9 tx_all:9 verbose tx_all:9 debug`.
API_KEY in the env selects Noise (see apiconn.py).
"""

import asyncio
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from aioesphomeapi import ButtonInfo  # noqa: E402
from apiconn import connect  # noqa: E402


async def main() -> None:
    c = await connect(sys.argv[1], tries=60)
    ents, _ = await c.list_entities_services()
    keys = {e.object_id: e.key for e in ents if isinstance(e, ButtonInfo)}
    for step in sys.argv[2:]:
        name, _, secs = step.partition(":")
        c.button_command(keys[name])
        await asyncio.sleep(float(secs or 1))
    await c.disconnect()
    print("pressed", len(sys.argv) - 2)


asyncio.run(main())
