"""list.py NAME: connect with bleak, print every service with its characteristics and read the 2A6E values."""

import asyncio
import json
import sys

from bleak import BleakClient, BleakScanner

SFX = "-0000-1000-8000-00805f9b34fb"


async def main(name):
    dev = await BleakScanner.find_device_by_name(name, timeout=40)
    async with BleakClient(dev, timeout=40) as c:
        tab = {}
        for s in c.services:
            k = f"{s.uuid[4:8] if s.uuid.endswith(SFX) and s.uuid.startswith('0000') else s.uuid[:8]}@{s.handle}"
            tab[k] = [ch.uuid[4:8] for ch in s.characteristics]
        print(json.dumps({"table": tab}), flush=True)
        vals = {}
        for s in c.services:
            if s.uuid.startswith("000018") and "1821" <= s.uuid[4:8] <= "182f":
                ch = s.characteristics[0] if s.characteristics else None
                try:
                    vals[s.uuid[4:8]] = (
                        await asyncio.wait_for(c.read_gatt_char(ch), 8)
                    ).hex()
                except Exception as e:  # noqa: BLE001
                    vals[s.uuid[4:8]] = f"{type(e).__name__}"
        print(json.dumps({"grown": vals}), flush=True)


asyncio.run(main(sys.argv[1]))
