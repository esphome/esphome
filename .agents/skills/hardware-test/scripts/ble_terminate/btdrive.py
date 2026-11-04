"""Connect to the peer through the proxy, read its services, idle, disconnect; print host timestamps per round.
Usage: btdrive.py PROXY KEY PEER_MAC ROUNDS IDLE_S"""

import asyncio
import sys
import time

from aioesphomeapi import APIClient, APIConnectionError


async def main(host: str, key: str, peer: str, rounds: int, idle: float) -> int:
    addr = int(peer.replace(":", ""), 16)
    cli = APIClient(host, 6053, None, noise_psk=key)
    await cli.connect(login=True)
    info = await cli.device_info()
    print(f"proxy {info.name} bt_mac {info.bluetooth_mac_address}", flush=True)
    seen = 0

    def adv(msg) -> None:
        nonlocal seen
        seen += sum(a.address == addr for a in msg.advertisements)

    # The proxy only connects to a device its scanner has seen.
    cli.subscribe_bluetooth_le_raw_advertisements(adv)
    await asyncio.sleep(5)
    print(f"peer advertisements seen in 5 s: {seen}", flush=True)
    if not seen:
        print("FAIL: the proxy does not see the peer", flush=True)
        await cli.disconnect()
        return 1
    done = 0
    for r in range(1, rounds + 1):
        state: asyncio.Queue = asyncio.Queue()

        def cb(
            connected: bool, mtu: int, err: int, state: asyncio.Queue = state
        ) -> None:
            state.put_nowait((connected, err, time.time()))

        t0 = time.time()
        try:
            cancel = await cli.bluetooth_device_connect(
                addr, cb, timeout=20, address_type=0, feature_flags=0xFF
            )
        except (APIConnectionError, TimeoutError) as e:
            print(f"R{r} CONNECT ERROR {type(e).__name__}", flush=True)
            await asyncio.sleep(9)
            continue
        try:
            c, err, t = await asyncio.wait_for(state.get(), 25)
        except TimeoutError:
            print(f"R{r} CONNECT TIMEOUT", flush=True)
            cancel()
            await asyncio.sleep(8)
            continue
        if not c:
            print(f"R{r} CONNECT FAIL err {err} after {t - t0:.2f}s", flush=True)
            cancel()
            await asyncio.sleep(8)
            continue
        try:
            n = len((await cli.bluetooth_gatt_get_services(addr)).services)
        except (APIConnectionError, TimeoutError) as e:
            n = f"err {e!r}"
        await asyncio.sleep(idle)
        td = time.time()
        await cli.bluetooth_device_disconnect(addr)
        try:
            c2, err2, t2 = await asyncio.wait_for(state.get(), 15)
            if c2 or not isinstance(n, int):
                # an unexpected connect state or a failed service read: the round did not run as intended
                print(
                    f"R{r} BAD ROUND services {n} connected {c2} err {err2}", flush=True
                )
            else:
                print(
                    f"R{r} services {n} DISC_CALL {td:.3f} DISC_DONE {t2:.3f} (+{t2 - td:.3f}s) err {err2}",
                    flush=True,
                )
                done += 1
        except TimeoutError:
            print(
                f"R{r} services {n} DISC_CALL {td:.3f} no disconnect state", flush=True
            )
        cancel()
        await asyncio.sleep(
            9
        )  # a peer that missed the terminate hits its 6 s supervision timeout in this gap
    await cli.disconnect()
    print(f"rounds completed {done} of {rounds}", flush=True)
    return 0 if done == rounds else 1


sys.exit(
    asyncio.run(
        main(
            sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), float(sys.argv[5])
        )
    )
)
