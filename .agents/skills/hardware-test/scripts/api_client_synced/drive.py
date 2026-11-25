"""Drive on_client_synced from aioesphomeapi clients.   env: API_KEY (see apiconn.py)

Usage: drive.py HOST MODE [N] [LO,HI,STEP]
  logs      one client that only subscribes to logs for 8 s
  states    one client that subscribes like HA; checks every initial state arrives before its synced event
  disc N    N clients that subscribe to states and send a disconnect request right away
  discsweep N LO,HI,STEP  N disc clients per wait of LO to HI ms (STEP) between subscribe and disconnect
  multi N   N state clients at once, then each reconnects once
  selfreboot D1,D2,...              a client subscribes to states and presses Restart D ms later
  hareboot TOKENFILE ENTRY D1,...   reload the Home Assistant entry ENTRY (HA_URL env), press Restart D ms later
Prints RESULT lines; the reboot modes wait for the device after each reboot and print RUN lines with a wall clock.
"""

import asyncio
import contextlib
import functools
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "mqtt" / "ha_discovery"))
from aioesphomeapi import (  # noqa: E402
    APIClient,
    ButtonInfo,
    EntityInfo,
    EventInfo,
    HomeassistantServiceCall,
    LogLevel,
)
from apiconn import connect, log_text  # noqa: E402
from ha_rest import req_or_exit  # noqa: E402

HOST = sys.argv[1]
MODE = sys.argv[2]


def mk(name: str) -> APIClient:
    return connect(HOST, client_info=name)


def ts() -> str:
    return time.strftime("%H:%M:%S") + f".{int(time.time() * 1000) % 1000:03d}"


async def logs_client() -> None:
    c = await mk("hwtest-logs")
    lines: list[str] = []
    c.subscribe_logs(
        lambda m: lines.append(log_text(m)),
        log_level=LogLevel.LOG_LEVEL_DEBUG,
    )
    await asyncio.sleep(8)
    await c.disconnect()
    print(f"RESULT logs client done, {len(lines)} log lines")


async def states_client(name: str, hold: float = 4) -> dict:
    c = await mk(name)
    entities, _ = await c.list_entities_services()
    stateful = {
        e.key
        for e in entities
        if isinstance(e, EntityInfo) and not isinstance(e, (ButtonInfo, EventInfo))
    }
    received: list = []
    t0 = time.monotonic()
    got = asyncio.Event()

    def on_state(s) -> None:
        received.append(("state", s.key, time.monotonic() - t0))

    def on_call(call: HomeassistantServiceCall) -> None:
        if call.is_event and call.service == "esphome.client_synced":
            received.append(("event", call.data.get("client"), time.monotonic() - t0))
            if call.data.get("client") == name:
                got.set()
        else:
            received.append(("action", call.service, time.monotonic() - t0))

    c.subscribe_home_assistant_states_and_services(
        on_state=on_state, on_service_call=on_call, on_state_sub=lambda *_: None
    )
    with contextlib.suppress(TimeoutError):
        await asyncio.wait_for(got.wait(), 10)
    await asyncio.sleep(hold)
    await c.disconnect()
    own = [i for i, r in enumerate(received) if r[0] == "event" and r[1] == name]
    first = own[0] if own else len(received)
    before = {r[1] for r in received[:first] if r[0] == "state"}
    after = [r for r in received[first:] if r[0] == "state" and r[1] not in before]
    res = {
        "name": name,
        "entities": len(stateful),
        "states_before_event": len(before),
        "missing_before": len(stateful - before),
        "new_states_after": len(after),
        "own_events": len(own),
        "event_at_s": round(received[first][2], 3) if own else None,
        "last_state_before_s": round(
            max((r[2] for r in received[:first] if r[0] == "state"), default=0), 3
        ),
    }
    print("RESULT states", res)
    return res


async def disc_client(i: int, delay_ms: float = 0) -> None:
    c = await mk(f"hwtest-disc-{i:02d}")
    c.subscribe_states(lambda s: None)
    if delay_ms:
        await asyncio.sleep(delay_ms / 1000)
    # with no delay, DisconnectRequest goes out in the same loop pass as SubscribeStates
    await c.disconnect()


async def close(c: APIClient) -> None:
    await asyncio.sleep(2)
    with contextlib.suppress(Exception):  # the device is rebooting
        await c.disconnect()


async def selfreboot(key: int, d: int) -> None:
    c = await mk(f"hwtest-rb-{d:03d}")
    c.subscribe_states(lambda s: None)
    if d:
        await asyncio.sleep(d / 1000)
    print(f"RUN {ts()} selfreboot d={d} press", flush=True)
    c.button_command(key)
    await close(c)


async def hareboot(tok: str, entry: str, key: int, d: int) -> None:
    c = await mk("hwtest-press")
    await asyncio.sleep(1)
    print(f"RUN {ts()} hareboot d={d} reload", flush=True)
    await asyncio.to_thread(
        req_or_exit, f"/api/config/config_entries/entry/{entry}/reload", {}, token=tok
    )
    if d:
        await asyncio.sleep(d / 1000)
    print(f"RUN {ts()} hareboot d={d} press", flush=True)
    c.button_command(key)
    await close(c)


async def reboots(run, delays: str) -> None:
    c = await mk("hwtest-keyprobe")
    entities, _ = await c.list_entities_services()
    await c.disconnect()
    key = next(
        e.key for e in entities if isinstance(e, ButtonInfo) and e.name == "Restart"
    )
    for d in (int(x) for x in delays.split(",")):
        await run(key, d)
        await asyncio.sleep(3)
        await (
            await connect(HOST, tries=60, timeout=3, client_info="hwtest-probe")
        ).disconnect()
        await asyncio.sleep(8)  # let Home Assistant reconnect and sync
    print(f"RUN {ts()} done", flush=True)


async def main() -> None:
    n = (
        int(sys.argv[3])
        if len(sys.argv) > 3 and MODE in ("disc", "discsweep", "multi")
        else 1
    )
    if MODE == "logs":
        await logs_client()
    elif MODE == "states":
        await states_client("hwtest-states")
    elif MODE == "disc":
        for i in range(n):
            await disc_client(i)
            await asyncio.sleep(1.5)
        print(f"RESULT disc {n} clients done")
    elif MODE == "discsweep":
        i = 0
        lo, hi, st = (
            (int(x) for x in sys.argv[4].split(","))
            if len(sys.argv) > 4
            else (0, 400, 10)
        )
        for d in range(lo, hi, st):
            for _ in range(n):
                await disc_client(i, d)
                i += 1
                await asyncio.sleep(0.7)
        print(f"RESULT discsweep {i} clients done")
    elif MODE == "multi":
        for rnd in range(2):
            rs = await asyncio.gather(
                *(states_client(f"hwtest-multi-{rnd}-{i}", hold=3) for i in range(n))
            )
            ok = all(r["own_events"] == 1 and r["missing_before"] == 0 for r in rs)
            print(f"RESULT multi round {rnd}: {'ok' if ok else 'BAD'}")
            await asyncio.sleep(3)
    elif MODE == "selfreboot":
        await reboots(selfreboot, sys.argv[3])
    elif MODE == "hareboot":
        tok = Path(sys.argv[3]).read_text(encoding="utf-8").strip()
        await reboots(functools.partial(hareboot, tok, sys.argv[4]), sys.argv[5])
    else:
        sys.exit(f"unknown mode {MODE}")


asyncio.run(main())
