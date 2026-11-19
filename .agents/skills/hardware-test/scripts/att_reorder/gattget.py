"""Connect to PEER_MAC through the proxy like Home Assistant (v3 without cache) and print the services it returns.
usage: gattget.py PROXY PEER_MAC ROUNDS   env: API_KEY (the proxy's api encryption key)"""

import asyncio
import os
import sys

from aioesphomeapi import APIClient


async def main(host: str, peer: str, rounds: int) -> None:
    addr = int(peer.replace(":", ""), 16)
    cli = APIClient(host, 6053, None, noise_psk=os.environ["API_KEY"])
    await cli.connect(login=True)
    seen = asyncio.Event()

    def adv(msg) -> None:
        if any(a.address == addr for a in msg.advertisements):
            seen.set()

    cli.subscribe_bluetooth_le_raw_advertisements(adv)
    await asyncio.wait_for(seen.wait(), 30)
    failed = 0
    for r in range(1, rounds + 1):
        state: asyncio.Queue = asyncio.Queue()
        cancel = await cli.bluetooth_device_connect(
            addr,
            lambda c, mtu, err, q=state: q.put_nowait((c, mtu, err)),
            timeout=20,
            address_type=0,
            feature_flags=0xFF,
            has_cache=False,
        )
        c, mtu, err = await asyncio.wait_for(state.get(), 25)
        if not c:
            print(f"R{r} connect failed err={err}", flush=True)
            failed += 1
            cancel()
            await asyncio.sleep(5)
            continue
        svcs = await cli.bluetooth_gatt_get_services(addr)
        sfx = "-0000-1000-8000-00805f9b34fb"
        out = [
            f"{s.uuid[4:8] if s.uuid.endswith(sfx) else s.uuid}({len(s.characteristics)})"
            for s in svcs.services
        ]
        print(f"R{r} mtu={mtu} services: {' '.join(out)}", flush=True)
        await cli.bluetooth_device_disconnect(addr)
        cancel()
        await asyncio.sleep(5)
    await cli.disconnect()
    if failed:
        sys.exit(f"{failed} of {rounds} rounds did not connect")


asyncio.run(main(sys.argv[1], sys.argv[2], int(sys.argv[3])))
