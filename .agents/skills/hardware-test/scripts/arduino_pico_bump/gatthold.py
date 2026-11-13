"""Through a bluetooth_proxy: connect to the bnsrv GATT server, read both characteristics, enable notify on both
(writing the CCCD like Home Assistant), hold for HOLD seconds re-reading every 20 s, then disconnect.
usage: gatthold.py PROXY PEER_MAC HOLD   env: API_KEY. Prints counts; exits 1 on a lost link or failed read."""

import asyncio
import os
import sys
import time

from aioesphomeapi import APIClient

NUM = "5ec10001-0000-1000-8000-00805f9b34fb"
TXT = "5ec10002-0000-1000-8000-00805f9b34fb"


async def main(host: str, peer: str, hold: float) -> None:
    addr = int(peer.replace(":", ""), 16)
    cli = APIClient(host, 6053, None, noise_psk=os.environ["API_KEY"])
    await cli.connect(login=True)
    seen = asyncio.Event()
    cli.subscribe_bluetooth_le_raw_advertisements(
        lambda m: (
            seen.set() if any(a.address == addr for a in m.advertisements) else None
        )
    )
    t = time.monotonic()
    await asyncio.wait_for(seen.wait(), 60)
    print(f"advert seen after {time.monotonic() - t:.1f}s", flush=True)
    state: asyncio.Queue = asyncio.Queue()
    lost = []
    first = True
    closing = False
    empty = 0
    samples: list[bytes] = []

    def on_state(c: bool, mtu: int, err: int) -> None:
        nonlocal first, closing
        if first:
            first = False
            state.put_nowait((c, mtu, err))
        elif not c and not closing:
            lost.append((time.monotonic(), err))
            print(f"LINK LOST err={err}", flush=True)

    t = time.monotonic()
    cancel = await cli.bluetooth_device_connect(
        addr, on_state, timeout=30, address_type=0, feature_flags=0xFF, has_cache=False
    )
    c, mtu, err = await asyncio.wait_for(state.get(), 35)
    if not c:
        sys.exit(f"FAIL connect err={err}")
    print(f"connected in {time.monotonic() - t:.1f}s mtu={mtu}", flush=True)
    svcs = await cli.bluetooth_gatt_get_services(addr)
    chars = {ch.uuid.lower(): ch for s in svcs.services for ch in s.characteristics}
    print("uuids", sorted(chars), flush=True)
    print(f"services {len(svcs.services)} characteristics {len(chars)}", flush=True)
    # the rp2 proxy reports a 32-bit UUID on the Bluetooth base as its low 16 bits (5ec10001 -> 00000001)
    for u in (NUM, TXT):
        if u not in chars and "0000" + u[4:] in chars:
            print(f"UUID {u[:8]} reported as {'0000' + u[4:8]}", flush=True)
            chars[u] = chars["0000" + u[4:]]
    got = {NUM: 0, TXT: 0}
    reads = fails = 0
    for u in (NUM, TXT):
        ch = chars[u]
        await cli.bluetooth_gatt_start_notify(
            addr, ch.handle, lambda h, d, u=u: got.__setitem__(u, got[u] + 1)
        )
        cccd = next(d for d in ch.descriptors if d.uuid.startswith("00002902"))
        await cli.bluetooth_gatt_write_descriptor(addr, cccd.handle, b"\x01\x00")
    end = time.monotonic() + hold
    while time.monotonic() < end and not lost:
        for u in (NUM, TXT):
            try:
                v = bytes(await cli.bluetooth_gatt_read(addr, chars[u].handle))
                reads += 1
                empty += not v
                if v and len(samples) < 4:
                    samples.append(v)
            except Exception as e:  # noqa: BLE001
                fails += 1
                print(f"read {u[4:8]} FAILED {type(e).__name__}: {e}", flush=True)
        await asyncio.sleep(20)
    print(
        f"held {hold:.0f}s: notifications num={got[NUM]} txt={got[TXT]} reads ok={reads} failed={fails} "
        f"link losses={len(lost)} empty reads={empty} samples={samples}",
        flush=True,
    )
    closing = True
    await cli.bluetooth_device_disconnect(addr)
    await asyncio.sleep(2)
    cancel()
    await cli.disconnect()
    if lost or fails or not got[NUM]:
        sys.exit("FAIL")
    print("PASS", flush=True)


asyncio.run(main(sys.argv[1], sys.argv[2], float(sys.argv[3])))
