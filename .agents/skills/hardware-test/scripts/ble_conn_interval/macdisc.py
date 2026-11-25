"""macdisc.py NAME ROUNDS: connect from this computer with bleak and time connect plus full service discovery (a reference for a
real device's table size and cold discovery time; CoreBluetooth caches after the first round)"""

import asyncio
import sys
import time

from bleak import BleakClient, BleakScanner


async def main():
    name, rounds = sys.argv[1], int(sys.argv[2])
    dev = await BleakScanner.find_device_by_name(name, timeout=30)
    if dev is None:
        print("not found")
        return
    for r in range(rounds):
        t0 = time.monotonic()
        try:
            async with BleakClient(dev, timeout=30) as c:
                t1 = time.monotonic()
                svcs = c.services
                nc = sum(len(s.characteristics) for s in svcs)
                nd = sum(len(ch.descriptors) for s in svcs for ch in s.characteristics)
                print(
                    f"round {r} connect+discovery {t1 - t0:.2f}s services={len(list(svcs))} chars={nc} descs={nd}",
                    flush=True,
                )
        except Exception as e:  # noqa: BLE001
            print(
                f"round {r} failed after {time.monotonic() - t0:.2f}s: {type(e).__name__}: {e}",
                flush=True,
            )
        await asyncio.sleep(3)


asyncio.run(main())
