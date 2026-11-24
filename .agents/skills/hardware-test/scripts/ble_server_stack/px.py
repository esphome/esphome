"""Through a bluetooth_proxy: px.py HOST NAME MODE [CMDS]. env API_KEY.
MODE table: cold (cache cleared) connect, full table, reads of any grown 0x1821.. services (grow.py), reads, write,
descriptor write, battery read + CCCD.
MODE rt: for each control command in CMDS (comma list, 0 = none) write it, disconnect, clear cache, reconnect and
report whether 0x181A is in the table and what 2A6E reads."""

import asyncio
import contextlib
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, enable_notify  # noqa: E402

SFX = "-0000-1000-8000-00805f9b34fb"


def short(u: str) -> str:
    if u.lower().endswith(SFX) and u.lower().startswith("0000"):
        return u[4:8]
    return u[:8]


def adv_name(data):
    i = 0
    while i < len(data):
        ln = data[i]
        if ln == 0 or i + 1 + ln > len(data):
            break
        if data[i + 1] in (8, 9):
            return data[i + 2 : i + 1 + ln].decode(errors="replace")
        i += 1 + ln
    return None


async def find(cli, name):
    fut = asyncio.get_running_loop().create_future()

    def cb(m):
        for a in m.advertisements:
            if adv_name(a.data) == name and not fut.done():
                fut.set_result((a.address, a.address_type))

    # keep the subscription open: without an advert subscriber the proxy drops links and requests
    cli.subscribe_bluetooth_le_raw_advertisements(cb)
    return await asyncio.wait_for(fut, 60)


async def link(cli, addr, atype):
    # a proxy with scan window = interval is Wi-Fi starved and drops replies now and then; retry both steps
    for _ in range(4):
        try:
            if (await cli.bluetooth_device_clear_cache(addr)).success:
                break
        except Exception as e:  # noqa: BLE001
            print(f"clear cache retry: {e}", file=sys.stderr, flush=True)
        await asyncio.sleep(3)
    else:
        raise RuntimeError("clear cache failed 4 times")
    last = None
    for _ in range(3):
        q = asyncio.Queue()
        cancel = await cli.bluetooth_device_connect(
            addr,
            lambda c, m, e, q=q: q.put_nowait((c, m, e)),
            timeout=30,
            address_type=atype,
            feature_flags=0xFF,
            has_cache=False,
        )
        try:
            c, m, e = await asyncio.wait_for(q.get(), 40)
        except TimeoutError:
            c, e = False, -1
        if c:
            try:
                svcs = await cli.bluetooth_gatt_get_services(addr)
                return cancel, svcs.services
            except Exception as ex:  # noqa: BLE001
                e = str(ex)
        last = e
        print(f"connect retry: {e}", file=sys.stderr, flush=True)
        cancel()
        with contextlib.suppress(Exception):
            await cli.bluetooth_device_disconnect(addr)
        await asyncio.sleep(3)
    raise RuntimeError(f"connect failed: {last}")


def find_char(services, cu):
    for s in services:
        for ch in s.characteristics:
            if ch.uuid.lower() == cu:
                return ch
    return None


async def drop(cli, addr, cancel):
    await cli.bluetooth_device_disconnect(addr)
    cancel()
    await asyncio.sleep(2)


async def main(host, name, mode, cmds):
    cli = await connect(host)
    addr, atype = await find(cli, name)
    out = {"mode": mode}
    if mode == "table":
        cancel, services = await link(cli, addr, atype)
        out["table"] = {
            short(s.uuid): {
                short(c.uuid): [short(d.uuid) for d in c.descriptors]
                for c in s.characteristics
            }
            for s in services
        }
        print(json.dumps({"early_table": out["table"]}), flush=True)
        grown = {}
        for s in services:
            if short(s.uuid).startswith("182") and short(s.uuid) != "1820":
                ch = s.characteristics[0]
                try:
                    grown[short(s.uuid)] = bytes(
                        await asyncio.wait_for(
                            cli.bluetooth_gatt_read(addr, ch.handle), 10
                        )
                    ).hex()
                except Exception as e:  # noqa: BLE001
                    grown[short(s.uuid)] = type(e).__name__
        print(json.dumps({"grown": grown}), flush=True)
        out["grown"] = grown

        def U(x: str) -> str:
            return f"5e5e{x}{SFX}"

        out["read"] = bytes(
            await cli.bluetooth_gatt_read(addr, find_char(services, U("0001")).handle)
        ).decode()
        dc = find_char(services, U("0004"))
        out["read_desc_char"] = bytes(
            await cli.bluetooth_gatt_read(addr, dc.handle)
        ).hex()
        dd = next(d for d in dc.descriptors if d.uuid.lower().startswith("5e5eff01"))
        out["desc"] = bytes(
            await cli.bluetooth_gatt_read_descriptor(addr, dd.handle)
        ).hex()
        notes = []
        await enable_notify(
            cli,
            addr,
            find_char(services, U("0002")),
            lambda h, d: notes.append(bytes(d).hex()),
        )
        await cli.bluetooth_gatt_write(
            addr, find_char(services, U("0003")).handle, b"\x5a\x5b\x5c", True
        )
        await asyncio.sleep(1.5)
        out["notifies"] = notes
        out["set_value_readback"] = bytes(
            await cli.bluetooth_gatt_read(addr, find_char(services, U("0002")).handle)
        ).hex()
        await cli.bluetooth_gatt_write_descriptor(addr, dd.handle, b"\x6a\x6b")
        out["desc_after"] = bytes(
            await cli.bluetooth_gatt_read_descriptor(addr, dd.handle)
        ).hex()
        lvl = find_char(services, "00002a19" + SFX)
        if lvl is not None:
            out["battery_level"] = bytes(
                await cli.bluetooth_gatt_read(addr, lvl.handle)
            ).hex()
            out["battery_cccd_written"] = await enable_notify(
                cli, addr, lvl, lambda h, d: None
            )
        await drop(cli, addr, cancel)
    else:
        steps = []
        for cmd in [int(x) for x in cmds.split(",")]:
            cancel, services = await link(cli, addr, atype)
            if cmd:
                await cli.bluetooth_gatt_write(
                    addr,
                    find_char(services, f"5e5e00c0{SFX}").handle,
                    bytes([cmd]),
                    True,
                )
                await asyncio.sleep(1.5)
            await drop(cli, addr, cancel)
            cancel, services = await link(cli, addr, atype)
            ess = [s for s in services if short(s.uuid) == "181a"]
            st = {
                "cmd": cmd,
                "svcs": sorted(short(s.uuid) for s in services),
                "ess": len(ess),
            }
            if ess:
                ch = find_char(services, "00002a6e" + SFX)
                st["2a6e"] = (
                    bytes(await cli.bluetooth_gatt_read(addr, ch.handle)).hex()
                    if ch
                    else None
                )
            steps.append(st)
            print(json.dumps(st), flush=True)
            await drop(cli, addr, cancel)
        out["steps"] = steps
    print(json.dumps(out), flush=True)
    with contextlib.suppress(Exception):
        await cli.disconnect()


if __name__ == "__main__":
    asyncio.run(
        main(
            sys.argv[1],
            sys.argv[2],
            sys.argv[3],
            sys.argv[4] if len(sys.argv) > 4 else "0",
        )
    )
