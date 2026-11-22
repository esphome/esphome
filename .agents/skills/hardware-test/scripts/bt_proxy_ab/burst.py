"""burst.py HOST RUNS OUT [COUNT PER_TICK]: notification burst through a Bluetooth proxy (esphome#22955).

Connects the burst server (PEER_L, built with gen_peri.py --burst) through the proxy, subscribes fe01 (2 byte
sequence notifications) and fe02 (final notification), triggers a burst with a write to fe03, and counts what arrives.
One JSON line per run in OUT.
env API_KEY (empty for plaintext), LOGS=1 to record the proxy's INFO log lines (the NDROP line of ndrop.hpp),
WARNONLY=1 to record warnings only.
"""

import asyncio
import contextlib
import os
import pathlib
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
sys.path[:0] = [str(HERE), str(HERE.parent)]  # peers.py here, apiconn.py one level up
from aioesphomeapi import LogLevel  # noqa: E402
from apiconn import connect, log_text  # noqa: E402
from drive import D, L, cccd  # noqa: E402

FE01 = "5ec2fe01-0000-1000-8000-00805f9b34fb"
FE02 = "5ec2fe02-0000-1000-8000-00805f9b34fb"
FE03 = "5ec2fe03-0000-1000-8000-00805f9b34fb"


async def main() -> None:
    host, runs, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    count = int(sys.argv[4]) if len(sys.argv) > 4 else 1000
    per = int(sys.argv[5]) if len(sys.argv) > 5 else 3
    if not 0 < per < 256 or not 0 < count < 65536:
        sys.exit("COUNT must be 1-65535 and PER_TICK 1-255")
    cli = await connect(host)
    d = D(cli, out)
    ndrop: list[str] = []
    logs = os.environ.get("LOGS") == "1"
    if logs:

        def on_log(m) -> None:
            if "NDROP" in (t := log_text(m)) or "notify data" in t:
                ndrop.append(t)

        cli.subscribe_logs(
            on_log,
            log_level=LogLevel.LOG_LEVEL_WARN
            if os.environ.get("WARNONLY")
            else LogLevel.LOG_LEVEL_INFO,
        )
    await d.start([L])
    addr = d.peers[L][0]
    # a stale proxy GATT cache from an older server table must not answer
    if not await d.clear(L):
        d.rec(test="burst", result="clear_failed")
        sys.exit(1)
    st = await d.connect(L, False, timeout=60)
    if not st["connected"]:
        d.rec(result="connect_failed", err=st.get("err"), exc=st.get("exc"))
        return
    svcs = await cli.bluetooth_gatt_get_services(addr)
    ch = {c.uuid: c for s in svcs.services for c in s.characteristics}
    if any(u not in ch for u in (FE01, FE02, FE03)) or None in (
        cccd(ch[FE01]),
        cccd(ch[FE02]),
    ):
        d.rec(test="burst", result="no_burst_service", hint="build PEER_L with --burst")
        await d.disconnect(L)
        sys.exit(1)
    seqs: list[tuple[float, int]] = []
    final: list[float] = []
    done = asyncio.Event()
    await cli.bluetooth_gatt_start_notify(
        addr,
        ch[FE01].handle,
        lambda h, x: seqs.append((time.monotonic(), x[0] | x[1] << 8)),
    )
    await cli.bluetooth_gatt_start_notify(
        addr, ch[FE02].handle, lambda h, x: (final.append(time.monotonic()), done.set())
    )
    for c, v in ((ch[FE01], b"\x01\x00"), (ch[FE02], b"\x02\x00")):
        await cli.bluetooth_gatt_write_descriptor(addr, cccd(c), v)
    for r in range(runs):
        seqs.clear()
        final.clear()
        ndrop.clear()
        done.clear()
        t0 = time.monotonic()
        await cli.bluetooth_gatt_write(
            addr,
            ch[FE03].handle,
            bytes([count >> 8, count & 0xFF, per]),
            True,
            timeout=20,
        )
        # the burst lasts count / (per / 15 ms); the final notification on fe02 ends it unless it was lost
        with contextlib.suppress(TimeoutError):
            await asyncio.wait_for(
                done.wait(), t0 + count / (per / 0.015) + 8 - time.monotonic()
            )
        if logs:
            await asyncio.sleep(3)  # ndrop.hpp prints after 2 s without notifications
        got = sorted({s for _, s in seqs})
        missing = count - len(got)
        runs_missing, longest, prev = 0, 0, -1
        for s in got + [count]:
            if s > prev + 1:
                runs_missing += 1
                longest = max(longest, s - prev - 1)
            prev = s
        span = (seqs[-1][0] - seqs[0][0]) if seqs else 0
        d.rec(
            test="burst",
            round=r,
            count=count,
            per_tick=per,
            received=len(got),
            missing=missing,
            loss_pct=round(100 * missing / count, 1),
            gap_runs=runs_missing,
            longest_gap=longest,
            final=bool(final),
            span_s=round(span, 2),
            rate=round(len(got) / span, 1) if span else None,
            lost_link=st["lost"][:1],
            ndrop=ndrop[-2:],
        )
        if st["lost"]:
            break
        await asyncio.sleep(3)
    await d.disconnect(L)
    with contextlib.suppress(Exception):
        await asyncio.wait_for(cli.disconnect(), 5)


if __name__ == "__main__":
    asyncio.run(main())
