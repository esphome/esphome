"""Home Assistant stand-in for a bluetooth_proxy scan soak (esphome#22770).

usage: ha.py HOST OUT SECONDS [--toggle SECS [--jitter SECS]] [--mode passive|active|none]

Subscribes to raw advertisements and the scanner state like Home Assistant, sets the scanner mode once after connect
(passive by default, as HA does for its proxies) and with --toggle flips active/passive every SECS seconds to stress
the stop/restart path. Writes one line per event to OUT: scanner state changes, a per-second advert count, mode
requests, connection drops. Reconnects after a drop (device reboot) and keeps going until SECONDS have passed.
API_KEY in the env selects Noise.
"""

import argparse
import asyncio
import pathlib
import random
import sys
import time

_here = pathlib.Path(__file__).resolve().parent
# apiconn.py: copied next to it, or the skill scripts dir
sys.path[:0] = [str(_here), str(_here.parent)]

from apiconn import FATAL, connect, stop_event  # noqa: E402


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("out")
    ap.add_argument("seconds", type=float)
    ap.add_argument("--toggle", type=float, default=0)
    ap.add_argument(
        "--jitter",
        type=float,
        default=0,
        help="random +- seconds added to each toggle period",
    )
    ap.add_argument("--mode", default="passive", choices=["passive", "active", "none"])
    a = ap.parse_args()
    asyncio.run(run(a))


async def run(a: argparse.Namespace) -> None:
    from aioesphomeapi import BluetoothScannerMode

    end = time.monotonic() + a.seconds
    f = pathlib.Path(a.out).open("a", encoding="utf-8", buffering=1)  # noqa: SIM115

    def w(s: str) -> None:
        f.write(
            f"{time.strftime('%H:%M:%S')}.{int(time.time() * 1000) % 1000:03d} {s}\n"
        )

    session = 0
    while time.monotonic() < end:
        session += 1
        stopped, on_stop = stop_event()
        # a try takes up to 11 s (10 s timeout, 1 s pause): bound the tries by the soak time left instead of
        # cancelling connect(), which could leave a half open client behind
        tries = max(1, int((end - time.monotonic()) // 11))
        try:
            c = await connect(a.host, tries=tries, on_stop=on_stop)
        except SystemExit as e:
            w(f"CONNECT FAILED {e}")
            # a wrong key or a plaintext/Noise mismatch never recovers: stop instead of soaking without a client
            if isinstance(e.__context__, FATAL):
                raise
            await asyncio.sleep(5)
            continue
        w(f"CONNECTED session={session}")
        adv = [0]
        macs: set[int] = set()

        def on_raw(resp, adv=adv, macs=macs) -> None:
            for x in resp.advertisements:
                adv[0] += 1
                macs.add(x.address)

        def on_state(st) -> None:
            w(f"SCANNER state={st.state.name} mode={st.mode.name}")

        unsub_raw = c.subscribe_bluetooth_le_raw_advertisements(on_raw)
        unsub_state = c.subscribe_bluetooth_scanner_state(on_state)
        if a.mode != "none":
            m = (
                BluetoothScannerMode.PASSIVE
                if a.mode == "passive"
                else BluetoothScannerMode.ACTIVE
            )
            c.bluetooth_scanner_set_mode(m)
            w(f"SET_MODE {a.mode}")
        last_toggle = time.monotonic()
        period = a.toggle
        active = a.mode == "active"
        last_report = time.monotonic()
        while time.monotonic() < end and not stopped.is_set():
            await asyncio.sleep(0.05)
            now = time.monotonic()
            if a.toggle and now - last_toggle >= period:
                active = not active
                c.bluetooth_scanner_set_mode(
                    BluetoothScannerMode.ACTIVE
                    if active
                    else BluetoothScannerMode.PASSIVE
                )
                w(f"SET_MODE {'active' if active else 'passive'}")
                last_toggle = now
                period = a.toggle + random.uniform(-a.jitter, a.jitter)
            if now - last_report >= 10:
                w(f"ADV {adv[0]} in {now - last_report:.1f}s devices={len(macs)}")
                adv[0] = 0
                macs.clear()
                last_report = now
        if stopped.is_set():
            w("DISCONNECTED")
            await asyncio.sleep(2)
            continue
        unsub_raw()
        unsub_state()
        await c.disconnect()
    w("DONE")


if __name__ == "__main__":
    main()
