"""Direct (bleak) GATT client rounds against bles2.yaml: gattc.py NAME ROUNDS TAG.
One JSON line per round on stdout, SUMMARY at the end. BATTERY=1 fails rounds without the Battery service."""

import asyncio
import json
import os
import sys

from bleak import BleakClient, BleakScanner
from list import short


def U(s: str) -> str:
    return f"5e5e{s}-0000-1000-8000-00805f9b34fb"


SVC, RD, NT, WR, DS, DESC = (
    U("0000"),
    U("0001"),
    U("0002"),
    U("0003"),
    U("0004"),
    "5e5eff01-0000-1000-8000-00805f9b34fb",
)
BAT, LVL = (
    "0000180f-0000-1000-8000-00805f9b34fb",
    "00002a19-0000-1000-8000-00805f9b34fb",
)
DIS = "0000180a-0000-1000-8000-00805f9b34fb"
IMPROV = "00467768-6228-2272-4663-277478268000"


async def one(name, rnd):
    r = {"round": rnd}
    dev = await BleakScanner.find_device_by_name(name, timeout=40)
    if dev is None:
        r["error"] = "not found"
        return r
    notes = []
    async with BleakClient(dev, timeout=30) as c:
        table = {}
        for s in c.services:
            table[short(s.uuid)] = {
                short(ch.uuid): [short(d.uuid) for d in ch.descriptors]
                for ch in s.characteristics
            }
        r["table"] = table
        r["dis"] = sorted(table.get("180a", {}))
        r["yaml_svc"] = SVC[:8] in table
        r["improv"] = IMPROV[:8] in table
        r["battery"] = "180f" in table
        r["read"] = (await c.read_gatt_char(RD)).decode(errors="replace")
        r["read_desc_char"] = (await c.read_gatt_char(DS)).hex()
        ch = c.services.get_characteristic(DS)
        dsc = next(d for d in ch.descriptors if d.uuid == DESC)
        r["desc_before"] = bytes(await c.read_gatt_descriptor(dsc.handle)).decode(
            errors="replace"
        )
        await c.start_notify(NT, lambda _, d: notes.append(bytes(d).hex()))
        payload = bytes([(0xA0 + rnd) & 0xFF, (0xB0 + rnd) & 0xFF, (0xC0 + rnd) & 0xFF])
        await c.write_gatt_char(WR, payload, response=True)
        await asyncio.sleep(1.5)
        r["written"] = payload.hex()
        r["notifies"] = notes
        r["set_value_readback"] = (await c.read_gatt_char(NT)).hex()
        dpay = bytes([(0xD0 + rnd) & 0xFF, (0xE0 + rnd) & 0xFF])
        await c.write_gatt_descriptor(dsc.handle, dpay)
        await asyncio.sleep(0.5)
        r["desc_written"] = dpay.hex()
        r["desc_after"] = bytes(await c.read_gatt_descriptor(dsc.handle)).hex()
        if r["battery"]:
            r["battery_level"] = (await c.read_gatt_char(LVL)).hex()
            lv = c.services.get_characteristic(LVL)
            r["battery_cccd"] = [short(d.uuid) for d in lv.descriptors]
            await c.start_notify(LVL, lambda _, d: None)
            r["battery_subscribed"] = True
            await c.stop_notify(LVL)
        await c.stop_notify(NT)
    return r


async def main(name, rounds, tag):
    ok = 0
    for i in range(1, rounds + 1):
        try:
            r = await one(name, i)
        except Exception as e:  # noqa: BLE001
            r = {"round": i, "error": f"{type(e).__name__}: {e}"}
        r["tag"] = tag
        print(json.dumps(r), flush=True)
        good = (
            "error" not in r
            and r["read"] == "hello"
            and r["read_desc_char"] == "07"
            and r["yaml_svc"]
            and r["desc_after"] == r["desc_written"]
            and r["written"] in r["notifies"]
            and r["set_value_readback"] == r["written"]
            and sorted(r["dis"]) == ["2a24", "2a26", "2a29"]
            and (r["battery"] or os.environ.get("BATTERY") != "1")
            and (
                not r["battery"]
                or (r["battery_level"] == "57" and r["battery_cccd"] == ["2902"])
            )
        )
        ok += good
        await asyncio.sleep(2)
    print(f"SUMMARY {tag} {ok}/{rounds} rounds good")


asyncio.run(main(sys.argv[1], int(sys.argv[2]), sys.argv[3]))
