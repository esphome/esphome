"""Direct (bleak) view of runtime service changes: rtmac.py NAME CMDS. Writes each command, reconnects, lists 0x181A."""

import asyncio
import json
import sys

from bleak import BleakClient, BleakScanner

CTRL = "5e5e00c0-0000-1000-8000-00805f9b34fb"


async def main(name, cmds):
    for cmd in [int(x) for x in cmds.split(",")]:
        dev = await BleakScanner.find_device_by_name(name, timeout=40)
        if cmd:
            async with BleakClient(dev, timeout=30) as c:
                await c.write_gatt_char(CTRL, bytes([cmd]), response=True)
                await asyncio.sleep(1.5)
        await asyncio.sleep(2)
        dev = await BleakScanner.find_device_by_name(name, timeout=40)
        async with BleakClient(dev, timeout=30) as c:
            ess = [s for s in c.services if s.uuid.startswith("0000181a")]
            st = {"cmd": cmd, "ess": len(ess)}
            if ess:
                ch = ess[0].get_characteristic("00002a6e-0000-1000-8000-00805f9b34fb")
                st["2a6e"] = (await c.read_gatt_char(ch)).hex() if ch else None
        print(json.dumps(st), flush=True)
        await asyncio.sleep(2)


asyncio.run(main(sys.argv[1], sys.argv[2]))
