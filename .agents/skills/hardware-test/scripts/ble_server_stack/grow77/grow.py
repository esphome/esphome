"""grow.py NAME N: one connection, N writes of 0x85 (each creates a service inside on_write), then reconnect and list
the 0x1821.. services and read their 2A6E. One JSON line."""

import asyncio
import json
import sys

from bleak import BleakClient, BleakScanner

CTRL = "5e5e00c0-0000-1000-8000-00805f9b34fb"
SFX = "-0000-1000-8000-00805f9b34fb"
CMD = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x85


async def main(name, n):
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
        dev = await BleakScanner.find_device_by_name(name, timeout=40)
        async with BleakClient(dev, timeout=30) as c:
            svcs = sorted(
                s.uuid[4:8]
                for s in c.services
                if s.uuid.endswith(SFX) and s.uuid.startswith("0000")
            )
            out["all16"] = svcs
            vals = {}
            for s in c.services:
                u = s.uuid[4:8]
                if s.uuid.startswith("000018") and "1821" <= u <= "182f":
                    ch = s.characteristics[0] if s.characteristics else None
                    vals[u] = (await c.read_gatt_char(ch)).hex() if ch else None
            out["grown"] = vals
    except Exception as e:  # noqa: BLE001
        out["list_error"] = f"{type(e).__name__}: {e}"
    print(json.dumps(out), flush=True)


asyncio.run(main(sys.argv[1], int(sys.argv[2])))
