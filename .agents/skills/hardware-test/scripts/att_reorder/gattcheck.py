"""Through the proxy (no cache): print the full GATT table, read FFF1/FFF2/FFF3 by handle, and enable notify on
FFF1, FFF2 and 2ACD (writing the CCCD the way Home Assistant does), counting what arrives per handle for 6 s.
usage: gattcheck.py PROXY PEER_MAC   env: API_KEY (the proxy's api encryption key)
Exits 1 when a characteristic or CCCD is missing, a read or notification carries another characteristic's value,
a notify fails, or a notifying characteristic sends nothing."""

import asyncio
import os
import sys

from aioesphomeapi import APIClient

SFX = "-0000-1000-8000-00805f9b34fb"


def short(u: str) -> str:
    return u[4:8] if u.endswith(SFX) else u


async def main(host: str, peer: str) -> None:
    addr = int(peer.replace(":", ""), 16)
    cli = APIClient(host, 6053, None, noise_psk=os.environ["API_KEY"])
    await cli.connect(login=True)
    seen = asyncio.Event()
    cli.subscribe_bluetooth_le_raw_advertisements(
        lambda m: (
            seen.set() if any(a.address == addr for a in m.advertisements) else None
        )
    )
    await asyncio.wait_for(seen.wait(), 30)
    state: asyncio.Queue = asyncio.Queue()
    cancel = await cli.bluetooth_device_connect(
        addr,
        lambda c, mtu, err: state.put_nowait((c, mtu, err)),
        timeout=20,
        address_type=0,
        feature_flags=0xFF,
        has_cache=False,
    )
    c, mtu, err = await asyncio.wait_for(state.get(), 25)
    if not c:
        sys.exit(f"connect failed err={err}")
    svcs = await cli.bluetooth_gatt_get_services(addr)
    chars = {}
    for s in svcs.services:
        print(f"SVC {short(s.uuid)} 0x{s.handle:04X}", flush=True)
        for ch in s.characteristics:
            chars[short(ch.uuid)] = ch
            print(
                f"  CHR {short(ch.uuid)} 0x{ch.handle:04X} props=0x{ch.properties:02X}",
                flush=True,
            )
            for d in ch.descriptors:
                print(f"    DSC {short(d.uuid)} 0x{d.handle:04X}", flush=True)
    wanted = ("fff1", "fff2", "fff3", "2acc", "2acd")
    problems = [f"{u} missing" for u in wanted if u not in chars]
    for u in ("fff1", "fff2", "fff3", "2acc"):
        if u in chars:
            v = bytes(await cli.bluetooth_gatt_read(addr, chars[u].handle))
            text = v.decode(errors="replace")
            print(f"READ {u} 0x{chars[u].handle:04X} -> {text}", flush=True)
            # every server value starts with its own UUID, so a mixed-up handle shows here
            if not text.startswith(f"{u}-"):
                problems.append(f"{u} read returned {text!r}")
    got: dict[int, list[str]] = {}

    def on_notify(handle: int, data: bytearray) -> None:
        got.setdefault(handle, []).append(bytes(data).decode(errors="replace"))

    for u in ("fff1", "fff2", "2acd"):
        if u in chars:
            try:
                await cli.bluetooth_gatt_start_notify(addr, chars[u].handle, on_notify)
                # like Home Assistant's client: write the first CCCD listed under the characteristic
                cccd = next(
                    (d for d in chars[u].descriptors if short(d.uuid) == "2902"), None
                )
                if cccd is None:
                    print(
                        f"NOTIFY {u} 0x{chars[u].handle:04X} NO CCCD in its descriptor list",
                        flush=True,
                    )
                    problems.append(f"{u} has no CCCD")
                    continue
                await cli.bluetooth_gatt_write_descriptor(
                    addr, cccd.handle, b"\x01\x00"
                )
                print(
                    f"NOTIFY {u} 0x{chars[u].handle:04X} enabled, CCCD 0x{cccd.handle:04X} written",
                    flush=True,
                )
            except Exception as exc:  # noqa: BLE001
                print(
                    f"NOTIFY {u} 0x{chars[u].handle:04X} FAILED {type(exc).__name__}: {exc}",
                    flush=True,
                )
                problems.append(f"{u} notify failed")
    await asyncio.sleep(6)
    if not got:
        print("GOT nothing", flush=True)
    for h, vals in sorted(got.items()):
        name = next((u for u, ch in chars.items() if ch.handle == h), "?")
        print(
            f"GOT 0x{h:04X} ({name}) x{len(vals)}: {vals[0]} .. {vals[-1]}", flush=True
        )
        problems.extend(
            f"0x{h:04X} ({name}) notified {v!r}"
            for v in vals
            if name == "?" or not v.startswith(f"{name}-")
        )
    # the server notifies FFF1 and 2ACD every second
    problems.extend(
        f"{u} sent nothing"
        for u in ("fff1", "2acd")
        if u in chars and chars[u].handle not in got
    )
    await cli.bluetooth_device_disconnect(addr)
    cancel()
    await cli.disconnect()
    if problems:
        sys.exit("FAIL: " + ", ".join(problems))
    print("PASS", flush=True)


asyncio.run(main(sys.argv[1], sys.argv[2]))
