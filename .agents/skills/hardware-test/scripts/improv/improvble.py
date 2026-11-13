"""Improv over BLE: find the device by name, send WIFI_SETTINGS and print state, errors and the RPC result URLs.
usage: improvble.py NAME SECRETS   (needs bleak; the device needs improv_ble authorizer: none and a next_url)"""

import asyncio
import pathlib
import sys

from bleak import BleakClient, BleakScanner
import yaml

BASE = "00467768-6228-2272-4663-2774782680"
STATE, ERROR, RPC, RESULT = BASE + "01", BASE + "02", BASE + "03", BASE + "04"
STATES = {1: "awaiting_auth", 2: "authorized", 3: "provisioning", 4: "provisioned"}


def pkt(cmd: int, data: bytes) -> bytes:
    b = bytes([cmd, len(data)]) + data
    return b + bytes([sum(b) & 0xFF])


async def main(name: str, secrets: str) -> None:
    with pathlib.Path(secrets).open(encoding="utf-8") as f:
        sec = yaml.safe_load(f)
    ssid, pw = sec["wifi_ssid"].encode(), sec["wifi_password"].encode()
    dev = await BleakScanner.find_device_by_name(name, timeout=30)
    if dev is None:
        sys.exit("device not found")
    loop = asyncio.get_running_loop()
    t0 = loop.time()
    done = asyncio.Event()
    errors: list = []
    states: list[int] = []
    async with BleakClient(dev) as c:

        def on_state(_, d: bytearray) -> None:
            if not d:
                errors.append("empty state")
                return
            states.append(d[0])
            print(f"{loop.time() - t0:6.2f} STATE {STATES.get(d[0], d[0])}", flush=True)

        def on_error(_, d: bytearray) -> None:
            if not d:
                errors.append("empty error")
            elif d[0]:
                errors.append(d[0])
                print(f"{loop.time() - t0:6.2f} ERROR {d[0]}", flush=True)

        def on_result(_, d: bytearray) -> None:
            if len(d) < 3 or d[1] != len(d) - 3 or sum(d[:-1]) & 0xFF != d[-1]:
                errors.append("bad result frame")
                print(
                    f"{loop.time() - t0:6.2f} BAD RESULT {bytes(d).hex()}", flush=True
                )
                return
            p, strs = bytes(d[2:-1]), []
            while p:
                strs.append(p[1 : 1 + p[0]].decode(errors="replace"))
                p = p[1 + p[0] :]
            print(f"{loop.time() - t0:6.2f} RESULT cmd={d[0]} {strs}", flush=True)
            if (
                d[0] == 1
            ):  # the WIFI_SETTINGS reply; the example sets next_url, so it carries a URL
                if not any(x.startswith("http") for x in strs):
                    errors.append("no URL in result")
                done.set()

        await c.start_notify(STATE, on_state)
        await c.start_notify(ERROR, on_error)
        await c.start_notify(RESULT, on_result)
        s = await c.read_gatt_char(STATE)
        print(f"initial state {STATES.get(s[0], s[0]) if s else 'empty'}")
        if not s or s[0] == 4:
            sys.exit(
                "FAIL: board already provisioned or no state; erase the flash or use a fresh config name first"
            )
        payload = bytes([len(ssid)]) + ssid + bytes([len(pw)]) + pw
        await c.write_gatt_char(RPC, pkt(1, payload), response=True)
        try:
            await asyncio.wait_for(done.wait(), 45)
        except TimeoutError:
            print("no result within 45 s")
        await asyncio.sleep(1)
        last = await c.read_gatt_char(STATE)
        states.append(last[0] if last else 0)
    if not done.is_set() or errors or states[-1] != 4:
        sys.exit(
            f"FAIL (errors {errors}, last state {STATES.get(states[-1], states[-1])})"
        )
    print("PASS")


asyncio.run(main(sys.argv[1], sys.argv[2]))
