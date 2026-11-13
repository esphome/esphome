"""List every service, characteristic and descriptor UUID a bluetooth_proxy reports for a peer, read each readable
characteristic, enable notify (CCCD write) on each notifying one and count 10 s of notifications, then disconnect.
usage: gattlist.py PROXY PEER_MAC OUT_JSON   env: API_KEY (empty for a plaintext API). Retries 0x3E connect failures."""

import asyncio
import json
import os
import pathlib
import sys

from aioesphomeapi import APIClient


async def once(host: str, addr: int) -> dict | None:
    cli = APIClient(host, 6053, None, noise_psk=os.environ.get("API_KEY") or None)
    await cli.connect(login=True)
    seen = asyncio.Event()
    cli.subscribe_bluetooth_le_raw_advertisements(
        lambda m: (
            seen.set() if any(a.address == addr for a in m.advertisements) else None
        )
    )
    await asyncio.wait_for(seen.wait(), 60)
    q: asyncio.Queue = asyncio.Queue()
    cancel = await cli.bluetooth_device_connect(
        addr,
        lambda c, mtu, err: q.put_nowait((c, err)),
        timeout=30,
        address_type=0,
        feature_flags=0xFF,
        has_cache=False,
    )
    c, err = await asyncio.wait_for(q.get(), 35)
    if not c:
        print(f"connect err={err}", flush=True)
        cancel()
        await cli.disconnect()
        return None
    svcs = await cli.bluetooth_gatt_get_services(addr)
    out = {"services": [], "reads": {}, "notify": {}}
    notif: dict[str, int] = {}
    for s in svcs.services:
        so = {"uuid": s.uuid, "characteristics": []}
        for ch in s.characteristics:
            so["characteristics"].append(
                {
                    "uuid": ch.uuid,
                    "props": ch.properties,
                    "descriptors": [d.uuid for d in ch.descriptors],
                }
            )
            if ch.properties & 0x02:
                try:
                    out["reads"][ch.uuid] = bytes(
                        await cli.bluetooth_gatt_read(addr, ch.handle)
                    ).hex()
                except Exception as e:  # noqa: BLE001
                    out["reads"][ch.uuid] = f"FAILED {type(e).__name__}"
            if ch.properties & 0x10:
                cccd = next(
                    (
                        d
                        for d in ch.descriptors
                        if d.uuid.lower().startswith("00002902")
                    ),
                    None,
                )
                notif[ch.uuid] = 0
                try:
                    await cli.bluetooth_gatt_start_notify(
                        addr,
                        ch.handle,
                        lambda h, d, u=ch.uuid: notif.__setitem__(u, notif[u] + 1),
                    )
                    if cccd is not None:
                        await cli.bluetooth_gatt_write_descriptor(
                            addr, cccd.handle, b"\x01\x00"
                        )
                except Exception as e:  # noqa: BLE001
                    notif[ch.uuid] = f"FAILED {type(e).__name__}"
        out["services"].append(so)
    await asyncio.sleep(10)
    out["notify"] = notif
    await cli.bluetooth_device_disconnect(addr)
    await asyncio.sleep(2)
    cancel()
    await cli.disconnect()
    return out


async def main(host: str, peer: str, path: str) -> None:
    addr = int(peer.replace(":", ""), 16)
    for _attempt in range(6):
        res = await once(host, addr)
        if res:
            break
        await asyncio.sleep(10)
    else:
        sys.exit("FAIL: no connection")
    pathlib.Path(path).write_text(json.dumps(res, indent=1), encoding="utf-8")
    for s in res["services"]:
        print(f"SVC {s['uuid']}")
        for c in s["characteristics"]:
            print(
                f"  CHR {c['uuid']} props=0x{c['props']:02X} read={res['reads'].get(c['uuid'], '-')}"
                f" notify={res['notify'].get(c['uuid'], '-')}"
            )
            for d in c["descriptors"]:
                print(f"    DSC {d}")


asyncio.run(main(sys.argv[1], sys.argv[2], sys.argv[3]))
