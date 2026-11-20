"""list.py NAME: connect with bleak, print every service with its characteristics and read the grown 0x1821..
services' 2A6E values. One JSON line."""

import asyncio
import json
import sys

from bleak import BleakClient, BleakScanner

SFX = "-0000-1000-8000-00805f9b34fb"


def short(u: str) -> str:
    return u[4:8] if u.endswith(SFX) and u.startswith("0000") else u[:8]


async def listing(name: str) -> dict:
    dev = await BleakScanner.find_device_by_name(name, timeout=40)
    async with BleakClient(dev, timeout=40) as c:
        tab = {
            f"{short(s.uuid)}@{s.handle}": [short(ch.uuid) for ch in s.characteristics]
            for s in c.services
        }
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
    return {"table": tab, "grown": vals}


if __name__ == "__main__":
    print(json.dumps(asyncio.run(listing(sys.argv[1]))), flush=True)
