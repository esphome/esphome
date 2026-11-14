"""Through a bluetooth_proxy: connect to the ble_notify GATT server (server.yaml), read both characteristics, enable
notify on both (writing the CCCD like Home Assistant), hold for HOLD seconds re-reading every 3 s, then disconnect.
usage: gatthold.py PROXY PEER_MAC HOLD   env: API_KEY. Prints counts; exits 1 on a lost link or failed read."""

import asyncio
import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import ble_connect, connect, enable_notify  # noqa: E402

NUM = "5ec10001-0000-1000-8000-00805f9b34fb"
TXT = "5ec10002-0000-1000-8000-00805f9b34fb"


async def main(host: str, peer: str, hold: float) -> None:
    addr = int(peer.replace(":", ""), 16)
    cli = await connect(host)
    lost: list[tuple[float, int]] = []
    closing = False

    def on_lost(err: int) -> None:
        if not closing:
            lost.append((time.monotonic(), err))
            print(f"LINK LOST err={err}", flush=True)

    t = time.monotonic()
    c, mtu, err, cancel = await ble_connect(cli, addr, on_lost)
    if not c:
        sys.exit(f"FAIL connect err={err}")
    print(f"connected in {time.monotonic() - t:.1f}s mtu={mtu}", flush=True)
    svcs = await cli.bluetooth_gatt_get_services(addr)
    chars = {ch.uuid.lower(): ch for s in svcs.services for ch in s.characteristics}
    print("uuids", sorted(chars), flush=True)
    print(f"services {len(svcs.services)} characteristics {len(chars)}", flush=True)
    # before #22679 the rp2 proxy reported a 32-bit base UUID as its low 16 bits (5ec10001 -> 00000001)
    for u in (NUM, TXT):
        if u not in chars and "0000" + u[4:] in chars:
            print(f"UUID {u[:8]} reported as {'0000' + u[4:8]}", flush=True)
            chars[u] = chars["0000" + u[4:]]
    got = {NUM: 0, TXT: 0}
    reads = fails = empty = 0
    samples: list[bytes] = []
    for u in (NUM, TXT):
        if not await enable_notify(
            cli, addr, chars[u], lambda h, d, u=u: got.__setitem__(u, got[u] + 1)
        ):
            sys.exit(f"FAIL: {u[:8]} lists no CCCD")
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
        # 3 s, so a hold covers several of the server's 10 s empty-value cycles
        await asyncio.sleep(3)
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
    # server.yaml empties both values on every 5th 2 s round, so about one read in five is legitimately empty; the
    # client cannot tell which round it hit, so only a majority of empty reads counts as a proxy returning no data
    if lost or fails or not got[NUM] or not got[TXT] or empty * 2 > reads:
        sys.exit("FAIL")
    print("PASS", flush=True)


asyncio.run(main(sys.argv[1], sys.argv[2], float(sys.argv[3])))
