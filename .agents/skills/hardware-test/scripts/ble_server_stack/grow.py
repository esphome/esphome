"""grow.py NAME N [CMD]: N writes of CMD (default 0x85: create a service inside on_write), one per connection, then
reconnect and list the services as list.py does. One JSON line."""

import asyncio
import json
import sys

from bleak import BleakClient, BleakScanner
from list import listing

CTRL = "5e5e00c0-0000-1000-8000-00805f9b34fb"
CMD = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x85


async def main(name: str, n: int) -> None:
    out = {"writes_ok": 0}
    # one write per connection: bleak on macOS drops its handles when the table changes, so a second write on the
    # same link never goes out
    for i in range(n):
        try:
            dev = await BleakScanner.find_device_by_name(name, timeout=40)
            async with BleakClient(dev, timeout=30) as c:
                await asyncio.wait_for(
                    c.write_gatt_char(CTRL, bytes([CMD]), response=True), 10
                )
                out["writes_ok"] += 1
                await asyncio.sleep(1.0)
        except Exception as e:  # noqa: BLE001
            out.setdefault("write_errors", []).append(
                f"{i + 1}: {type(e).__name__}: {e}"
            )
            if len(out["write_errors"]) >= 2:
                break
        await asyncio.sleep(2)
    await asyncio.sleep(4)
    try:
        out.update(await listing(name))
    except Exception as e:  # noqa: BLE001
        out["list_error"] = f"{type(e).__name__}: {e}"
    print(json.dumps(out), flush=True)


asyncio.run(main(sys.argv[1], int(sys.argv[2])))
