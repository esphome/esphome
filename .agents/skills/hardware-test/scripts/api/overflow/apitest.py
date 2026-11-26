"""apitest.py HOST OUT.json: Noise API check (entities, states, button) then a flood with a stalled reader
so the device's API overflow buffer fills and drains. Validates every received burst state."""

import asyncio
import gc
import json
import math
import pathlib
import socket
import sys
import time

from aioesphomeapi import SensorState, TextSensorState

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
import aioesphomeapi.connection as conn_mod
from apiconn import connect  # noqa: E402

HOST, OUT = sys.argv[1], sys.argv[2]
TL = 800


class SmallSock(socket.socket):
    def setsockopt(self, level, opt, *a):
        if level == socket.SOL_SOCKET and opt == socket.SO_RCVBUF and a[0] != 2048:
            return None
        return super().setsockopt(level, opt, *a)


async def start_small(addr_infos, **kw):
    fam, typ, proto, _, addr = addr_infos[0]
    s = SmallSock(fam, typ, proto)
    s.setblocking(False)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 2048)
    await asyncio.get_running_loop().sock_connect(s, addr)
    return s


conn_mod.aiohappyeyeballs.start_connection = start_small


def txt_ok(s, slot):
    if len(s) != TL or s[8] != "|" or not s[:8].isdigit():
        return None
    n = int(s[:8])
    for i in range(9, TL):
        if s[i] != chr(ord("A") + (n * 7 + slot * 3 + i) % 26):
            return None
    return n


async def main():
    res = {"host": HOST}
    stopped = {"t": None}

    async def on_stop(expected):
        stopped["t"] = time.monotonic()

    cli = await connect(HOST, on_stop=on_stop)
    info = await cli.device_info()
    res["device"] = info.name
    res["version"] = info.esphome_version
    ents, _ = await cli.list_entities_services()
    res["entities"] = len(ents)
    by_key = {e.key: e for e in ents}
    names = {e.object_id: e for e in ents}
    flood = names["flood"]
    bump = names["bump"]
    state = {}
    stats = {"states": 0, "bad_sensor": 0, "bad_text": 0, "burst_states": 0}

    def on_state(s):
        e = by_key.get(s.key)
        if e is None:
            return
        stats["states"] += 1
        oid = e.object_id
        if isinstance(s, SensorState) and oid.startswith("s") and oid[1:].isdigit():
            i = int(oid[1:])
            stats["burst_states"] += 1
            if not s.missing_state and (
                int(s.state) % 100 != i or s.state != int(s.state)
            ):
                stats["bad_sensor"] += 1
        if isinstance(s, TextSensorState) and oid.startswith("t") and oid[1:].isdigit():
            stats["burst_states"] += 1
            if not s.missing_state and txt_ok(s.state, int(oid[1:])) is None:
                stats["bad_text"] += 1
        state[oid] = s.state

    cli.subscribe_states(on_state)
    await asyncio.sleep(3)
    res["initial_states"] = len(state)
    b0 = state.get("bump_count")
    b0 = 0 if b0 is None or math.isnan(b0) else int(b0)
    for _ in range(5):
        cli.button_command(bump.key)
        await asyncio.sleep(0.5)
    await asyncio.sleep(3)
    b1 = state.get("bump_count")
    # a missing or NaN count gives no delta, which fails the run instead of raising before the JSON is written
    res["bump_delta"] = None if b1 is None or math.isnan(b1) else int(b1) - b0
    o0 = (
        state.get("ovf_enqueue"),
        state.get("ovf_drain"),
        state.get("ovf_enqueue_bytes"),
        state.get("ovf_enqueue_fail"),
    )
    sock = cli._connection._socket
    transport = next(
        o
        for o in gc.get_objects()
        if isinstance(o, asyncio.Transport)
        and not o.is_closing()
        and o.get_extra_info("socket") is not None
        and o.get_extra_info("socket").fileno() == sock.fileno()
    )
    cli.switch_command(flood.key, True)
    await asyncio.sleep(3)
    for _ in range(3):
        transport.pause_reading()
        await asyncio.sleep(6)
        transport.resume_reading()
        await asyncio.sleep(4)
    cli.switch_command(flood.key, False)
    await asyncio.sleep(6)
    fN = state.get("flood_n")
    final_ok = (
        fN is not None
        and not math.isnan(fN)
        and all(state.get(f"s{i}") == fN * 100 + i for i in range(60))
        and all(txt_ok(state.get(f"t{j}", ""), j) == int(fN) for j in range(4))
    )
    o1 = (
        state.get("ovf_enqueue"),
        state.get("ovf_drain"),
        state.get("ovf_enqueue_bytes"),
        state.get("ovf_enqueue_fail"),
    )
    res.update(stats)
    res["flood_n"] = fN
    res["final_states_match"] = final_ok
    # a counter with no state (missing or NaN) gives no delta, which fails the run
    for name, a, b in zip(
        (
            "ovf_enqueue_delta",
            "ovf_drain_delta",
            "ovf_enqueue_bytes_delta",
            "ovf_enqueue_fail_delta",
        ),
        o0,
        o1,
        strict=True,
    ):
        ok_ab = all(x is not None and not math.isnan(x) for x in (a, b))
        res[name] = b - a if ok_ab else None
    res["dropped"] = stopped["t"] is not None
    await cli.disconnect()
    res["pass"] = (
        not res["dropped"]
        and final_ok
        and res["bump_delta"] == 5
        and stats["bad_sensor"] == 0
        and stats["bad_text"] == 0
        and (res["ovf_enqueue_delta"] or 0) > 0
        and (res["ovf_drain_delta"] or 0) > 0
        # a refused enqueue loses a message
        and res["ovf_enqueue_fail_delta"] == 0
    )
    print(json.dumps(res))
    pathlib.Path(OUT).write_text(json.dumps(res, indent=1), encoding="utf-8")
    return 0 if res["pass"] else 1


sys.exit(asyncio.run(main()))
