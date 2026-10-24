"""Drive the FrameReader simulation builds over the API: press buttons, set selects/numbers, print states.
usage: frdrive.py HOST LOGFILE OP [OP ...]
  OP: list | press:NAME | sel:NAME=OPTION | num:NAME=VALUE | wait:SECONDS
"""

import asyncio
import math
import pathlib
import sys
import time

from aioesphomeapi import (
    APIClient,
    BinarySensorState,
    ButtonInfo,
    LogLevel,
    NumberInfo,
    SelectInfo,
    SensorInfo,
    SensorState,
)

HOST, LOGFILE = sys.argv[1], sys.argv[2]
OPS = sys.argv[3:]
T0 = time.time()
SKIP = {"Heap Free", "Loop Time"}


def stamp() -> str:
    return f"{time.time() - T0:7.3f}"


async def main() -> int:
    with pathlib.Path(LOGFILE).open("w", encoding="utf-8") as log:
        c = APIClient(HOST, 6053, None)
        await c.connect(login=True)
        info = await c.device_info()
        print(f"device {info.name} compiled {info.compilation_time}", flush=True)
        ents, _ = await c.list_entities_services()
        by_key = {e.key: e for e in ents}

        def find(kind: type, name: str):
            for e in ents:
                if isinstance(e, kind) and e.name == name:
                    return e
            sys.exit(f"no {kind.__name__} named {name!r}")

        def on_state(s) -> None:
            e = by_key.get(s.key)
            if e is None or e.name in SKIP:
                return
            if isinstance(s, SensorState) and isinstance(e, SensorInfo):
                v = "nan" if math.isnan(s.state) else f"{s.state:.2f}"
                line = f"{stamp()} SEN {e.name} = {v}"
            elif isinstance(s, BinarySensorState):
                line = f"{stamp()} BIN {e.name} = {s.state}"
            else:
                return
            print(line, flush=True)
            log.write(line + "\n")
            states.append(line)

        def on_log(msg) -> None:
            log.write(f"{stamp()} {msg.message.decode(errors='replace')}\n")
            log.flush()

        states: list[str] = []
        c.subscribe_states(on_state)
        c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_VERBOSE)
        await asyncio.sleep(2)
        states.clear()  # count only states published while the ops ran
        for op in OPS:
            kind, _, rest = op.partition(":")
            if kind == "list":
                for e in ents:
                    print(type(e).__name__, e.name)
                continue
            if kind == "wait":
                await asyncio.sleep(float(rest))
                continue
            line = f"{stamp()} >>> {op}"
            print(line, flush=True)
            log.write(line + "\n")
            if kind == "press":
                c.button_command(find(ButtonInfo, rest).key)
            elif kind == "sel":
                name, _, value = rest.partition("=")
                c.select_command(find(SelectInfo, name).key, value)
            elif kind == "num":
                name, _, value = rest.partition("=")
                c.number_command(find(NumberInfo, name).key, float(value))
            else:
                sys.exit(f"unknown op {op!r}")
            await asyncio.sleep(2.5)
        await asyncio.sleep(1)
        await c.disconnect()
    if any(not op.startswith(("list", "wait")) for op in OPS) and not states:
        print("no sensor state was published while the ops ran", flush=True)
        return 1
    return 0


sys.exit(asyncio.run(main()))
