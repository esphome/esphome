"""Play Home Assistant against a bluetooth_proxy.

usage: ha.py HOST KEY owner                connect to the test peripheral through the proxy, then idle forever
       ha.py HOST KEY swap ADDR            connect at once after subscribing, retry on the first advert, release
       ha.py HOST KEY observe SECONDS [connect ADDR]
                                           watch the peripheral's adverts and the proxy's slot report; optionally
                                           ask the proxy to connect to ADDR at the end and then release it
KEY of - is a plaintext API. The peripheral is found by its manufacturer data (FF FF 21 56, see peripheral.yaml).
"""

import asyncio
import sys
import time

from aioesphomeapi import APIClient, APIConnectionError

HOST, KEY, ROLE = sys.argv[1], sys.argv[2], sys.argv[3]
MARK = b"\xff\xff\x21\x56"


def log(*a: object) -> None:
    print(f"{time.time():.3f} {ROLE}", *a, flush=True)


async def find_peripheral(c: APIClient) -> tuple[int, int]:
    fut = asyncio.get_running_loop().create_future()

    def on_adv(resp) -> None:
        for adv in resp.advertisements:
            if MARK in adv.data and not fut.done():
                fut.set_result((adv.address, adv.address_type))

    # Stay subscribed like Home Assistant does: with no subscriber the proxy releases every link
    c.subscribe_bluetooth_le_raw_advertisements(on_adv)
    return await asyncio.wait_for(fut, 60)


async def owner(c: APIClient, flags: int) -> None:
    addr, atype = await find_peripheral(c)
    log(f"peripheral {addr:#x} seen, connecting")
    if not await connect_once(c, addr, flags, "owner", time.time(), atype):
        sys.exit("owner could not connect")
    log("OWNER HOLDS LINK")
    await asyncio.sleep(10**6)


async def connect_once(
    c: APIClient, addr: int, flags: int, label: str, t0: float, atype: int = 0
) -> bool:
    st = asyncio.get_running_loop().create_future()

    def on_state(connected: bool, mtu: int, err: int) -> None:
        log(
            f"{label} state connected={connected} mtu={mtu} err={err} at {time.time() - t0:.2f} s"
        )
        if not st.done():
            st.set_result(connected)

    log(f"{label} connect at {time.time() - t0:.2f} s")
    try:
        await c.bluetooth_device_connect(
            addr, on_state, timeout=30, feature_flags=flags, address_type=atype
        )
        return await asyncio.wait_for(st, 30)
    except (TimeoutError, APIConnectionError) as e:
        # TimeoutAPIError and a dropped API link are both APIConnectionError; anything else is a rig bug
        log(f"{label} error {e!r} at {time.time() - t0:.2f} s")
        return False


async def swap(c: APIClient, flags: int, addr: int) -> None:
    t0 = time.time()
    adv = asyncio.Event()

    def on_adv(resp) -> None:
        for a in resp.advertisements:
            if MARK in a.data and not adv.is_set():
                log(f"FIRST ADVERT after {time.time() - t0:.2f} s")
                adv.set()

    c.subscribe_bluetooth_le_raw_advertisements(on_adv)
    for attempt in (1, 2):
        if attempt == 2:
            await asyncio.wait_for(adv.wait(), 60)
        if await connect_once(c, addr, flags, f"attempt {attempt}", t0):
            log(f"CONNECTED on attempt {attempt} at {time.time() - t0:.2f} s")
            break
    else:
        log("both attempts failed, nothing to release")
        return
    await c.bluetooth_device_disconnect(addr)
    await asyncio.sleep(2)


async def observe(
    c: APIClient, flags: int, secs: float, connect_addr: int | None
) -> None:
    seen = {"n": 0, "first": None}

    def on_adv(resp) -> None:
        for adv in resp.advertisements:
            if MARK in adv.data:
                seen["n"] += 1
                if seen["first"] is None:
                    seen["first"] = time.time()
                    log("FIRST ADVERT from peripheral")

    c.subscribe_bluetooth_le_raw_advertisements(on_adv)
    end = time.time() + secs
    while time.time() < end:
        await asyncio.sleep(10)
        log(f"adverts so far {seen['n']}")
    if connect_addr is None:
        return
    log(f"asking proxy to connect {connect_addr:#x}")
    ok = await connect_once(c, connect_addr, flags, "observer", time.time())
    await c.bluetooth_device_disconnect(connect_addr)
    log("released")
    n0 = seen["n"]
    await asyncio.sleep(20)
    log(f"adverts in 20 s after release {seen['n'] - n0}")
    if not ok:
        sys.exit("observer connect failed")


async def main() -> None:
    c = APIClient(
        HOST,
        6053,
        None,
        noise_psk=None if KEY == "-" else KEY,
        client_info="Home Assistant test",
    )
    await c.connect(login=True)
    info = await c.device_info()
    flags = info.bluetooth_proxy_feature_flags_compat(c.api_version)
    log("api connected")
    c.subscribe_bluetooth_connections_free(
        lambda free, limit, alloc: log(
            f"slots free={free}/{limit} allocated={[hex(a) for a in alloc]}"
        )
    )
    if ROLE == "owner":
        await owner(c, flags)
    elif ROLE == "swap":
        await swap(c, flags, int(sys.argv[4], 16))
    else:
        addr = (
            int(sys.argv[6], 16)
            if len(sys.argv) > 6 and sys.argv[5] == "connect"
            else None
        )
        await observe(c, flags, float(sys.argv[4]), addr)
    await c.disconnect()


asyncio.run(main())
