"""Improv over BLE with a bad checksum first: improv2.py NAME SECRETS. Prints states, errors, result; exit 0 on PASS."""

import asyncio
import pathlib
import sys

from bleak import BleakClient, BleakScanner

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "improv"))
from improvcli import load_wifi_secrets

BASE = "00467768-6228-2272-4663-2774782680"
STATE, ERROR, RPC, RESULT = BASE + "01", BASE + "02", BASE + "03", BASE + "04"


def pkt(cmd, data, bad=False):
    b = bytes([cmd, len(data)]) + data
    return b + bytes([(sum(b) + (1 if bad else 0)) & 0xFF])


async def main(name, secrets):
    ssid, pw = (x.encode() for x in load_wifi_secrets(secrets))
    dev = await BleakScanner.find_device_by_name(name, timeout=40)
    if dev is None:
        sys.exit("FAIL: not found")
    states, errors, results = [], [], []
    done = asyncio.Event()
    async with BleakClient(dev) as c:
        await c.start_notify(
            STATE,
            lambda _, d: (states.append(d[0]), print("STATE", d.hex(), flush=True)),
        )
        await c.start_notify(
            ERROR,
            lambda _, d: (errors.append(d[0]), print("ERROR", d.hex(), flush=True)),
        )

        def on_res(_, d):
            results.append(bytes(d))
            print("RESULT", bytes(d), flush=True)
            if d and d[0] == 1:
                done.set()

        await c.start_notify(RESULT, on_res)
        s = await c.read_gatt_char(STATE)
        print("initial", s.hex())
        payload = bytes([len(ssid)]) + ssid + bytes([len(pw)]) + pw
        await c.write_gatt_char(RPC, pkt(1, payload, bad=True), response=True)
        await asyncio.sleep(2)
        await c.write_gatt_char(RPC, pkt(1, payload), response=True)
        try:
            await asyncio.wait_for(done.wait(), 45)
        except TimeoutError:
            print("no result")
        await asyncio.sleep(1)
        last = (await c.read_gatt_char(STATE))[0]
    ok = (
        done.is_set()
        and last == 4
        and errors[:1] == [1]
        and any(b"http" in r for r in results)
    )
    print(
        f"SUMMARY states={[f'{x:02x}' for x in states]} errors={[f'{x:02x}' for x in errors]} last={last:02x} "
        f"url={'yes' if any(b'http' in r for r in results) else 'no'} {'PASS' if ok else 'FAIL'}"
    )
    sys.exit(0 if ok else 1)


asyncio.run(main(sys.argv[1], sys.argv[2]))
