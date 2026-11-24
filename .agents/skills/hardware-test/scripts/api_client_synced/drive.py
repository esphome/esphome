"""Drive on_client_synced from aioesphomeapi clients (plaintext API on port 6053).

Usage: drive.py HOST MODE [N] [LO,HI,STEP]
  logs      one client that only subscribes to logs for 8 s
  states    one client that subscribes like HA; checks every initial state arrives before its synced event
  disc N    N clients that subscribe to states and send a disconnect request right away
  discsweep N LO,HI,STEP  N disc clients per wait of LO to HI ms (STEP) between subscribe and disconnect
  multi N   N state clients at once, then each reconnects once
Prints RESULT lines.
"""

import asyncio
import contextlib
import sys
import time

from aioesphomeapi import (
    APIClient,
    ButtonInfo,
    EntityInfo,
    EventInfo,
    HomeassistantServiceCall,
    LogLevel,
)

HOST = sys.argv[1]
MODE = sys.argv[2]
N = int(sys.argv[3]) if len(sys.argv) > 3 else 1


async def mk(name: str) -> APIClient:
    c = APIClient(HOST, 6053, None, client_info=name)
    await asyncio.wait_for(c.connect(login=True), 15)
    return c


async def logs_client() -> None:
    c = await mk("hwtest-logs")
    lines: list[str] = []
    c.subscribe_logs(
        lambda m: lines.append(m.message.decode(errors="replace")),
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
    await (
        c.disconnect()
    )  # DisconnectRequest goes out in the same loop pass as SubscribeStates


async def main() -> None:
    if MODE == "logs":
        await logs_client()
    elif MODE == "states":
        await states_client("hwtest-states")
    elif MODE == "disc":
        for i in range(N):
            await disc_client(i)
            await asyncio.sleep(1.5)
        print(f"RESULT disc {N} clients done")
    elif MODE == "discsweep":
        i = 0
        lo, hi, st = (
            (int(x) for x in sys.argv[4].split(","))
            if len(sys.argv) > 4
            else (0, 400, 10)
        )
        for d in range(lo, hi, st):
            for _ in range(N):
                await disc_client(i, d)
                i += 1
                await asyncio.sleep(0.7)
        print(f"RESULT discsweep {i} clients done")
    elif MODE == "multi":
        for rnd in range(2):
            rs = await asyncio.gather(
                *(states_client(f"hwtest-multi-{rnd}-{i}", hold=3) for i in range(N))
            )
            ok = all(r["own_events"] == 1 and r["missing_before"] == 0 for r in rs)
            print(f"RESULT multi round {rnd}: {'ok' if ok else 'BAD'}")
            await asyncio.sleep(3)


asyncio.run(main())
