"""Drive an LD2450 harness over the API: press buttons, set selects, print LD2450 state updates, save the log.

usage: lddrive.py HOST KEY LOGFILE OP [OP ...]   (KEY of - means a plaintext API)
  OP: list | press:NAME | sel:NAME=OPTION | wait:SECONDS
Steps are 2 s apart: the API log stream drops lines while the hub blocks in send_command_.
"""

import asyncio
from pathlib import Path
import sys
import time

from aioesphomeapi import (
    APIClient,
    ButtonInfo,
    LogLevel,
    NumberState,
    SelectInfo,
    SelectState,
    SensorInfo,
    SensorState,
    TextSensorInfo,
    TextSensorState,
)

HOST, KEY, LOGFILE = sys.argv[1], sys.argv[2], sys.argv[3]
OPS = sys.argv[4:]
T0 = time.time()
STEP_GAP_S = 2.0


def stamp() -> str:
    return f"{time.time() - T0:7.3f}"


async def run(c: APIClient, log) -> None:
    info = await c.device_info()
    print(
        f"device {info.name} esphome {info.esphome_version} compiled {info.compilation_time}"
    )
    ents, _ = await c.list_entities_services()
    by_key = {e.key: e for e in ents}
    buttons = {e.name: e for e in ents if isinstance(e, ButtonInfo)}
    selects = {e.name: e for e in ents if isinstance(e, SelectInfo)}

    def on_state(s) -> None:
        e = by_key.get(s.key)
        if e is None:
            return
        missing = " (missing)" if getattr(s, "missing_state", False) else ""
        if isinstance(s, TextSensorState):
            print(f"{stamp()} TXT {e.name} = {s.state!r}{missing}", flush=True)
        elif isinstance(s, NumberState):
            print(f"{stamp()} NUM {e.name} = {s.state:.0f}{missing}", flush=True)
        elif isinstance(s, SelectState):
            print(f"{stamp()} SEL {e.name} = {s.state!r}{missing}", flush=True)
        elif (
            isinstance(s, SensorState)
            and isinstance(e, SensorInfo)
            and "Count" in e.name
        ):
            print(f"{stamp()} CNT {e.name} = {s.state:.0f}", flush=True)

    def on_log(msg) -> None:
        log.write(f"{stamp()} {msg.message.decode(errors='replace')}\n")

    c.subscribe_states(on_state)
    c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_VERBOSE)
    await asyncio.sleep(1.5)
    for op in OPS:
        if op == "list":
            for n in sorted(buttons):
                print("button:", n)
            for n in sorted(selects):
                print("select:", n, selects[n].options)
            for e in ents:
                if isinstance(e, TextSensorInfo):
                    print("text_sensor:", e.name)
            continue
        kind, _, rest = op.partition(":")
        if kind == "wait":
            await asyncio.sleep(float(rest))
            continue
        if kind == "press":
            if rest not in buttons:
                sys.exit(f"no button named {rest!r}; have {sorted(buttons)}")
            print(f"{stamp()} >>> press {rest}", flush=True)
            log.write(f"{stamp()} >>> press {rest}\n")
            c.button_command(buttons[rest].key)
        elif kind == "sel":
            name, _, value = rest.partition("=")
            if name not in selects:
                sys.exit(f"no select named {name!r}; have {sorted(selects)}")
            print(f"{stamp()} >>> select {name} = {value}", flush=True)
            log.write(f"{stamp()} >>> select {name} = {value}\n")
            c.select_command(selects[name].key, value)
        else:
            sys.exit(f"unknown op {op!r}")
        await asyncio.sleep(STEP_GAP_S)
    await asyncio.sleep(1)


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=None if KEY == "-" else KEY)
    await c.connect(login=True)
    with Path(LOGFILE).open("w", encoding="utf-8") as log:
        await run(c, log)
    await c.disconnect()


asyncio.run(main())
