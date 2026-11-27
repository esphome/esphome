"""Drive LD2450 numbers and selects over the API while printing every LD2450 state update.
usage: zonedrive.py HOST KEY(- for plaintext) OP [OP ...]
  OP: set:NAME=VALUE | sel:NAME=OPTION | wait:SECONDS | list
Every state update of an entity whose name contains "LD2450" is printed with a timestamp.
"""

import asyncio
import sys
import time

from aioesphomeapi import (
    APIClient,
    NumberInfo,
    NumberState,
    SelectInfo,
    SelectState,
    SensorInfo,
    SensorState,
)

HOST, KEY = sys.argv[1], sys.argv[2]
OPS = sys.argv[3:]
T0 = time.time()


def stamp() -> str:
    return f"{time.time() - T0:7.3f}"


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=None if KEY == "-" else KEY)
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    by_key = {e.key: e for e in ents}
    numbers = {e.name: e for e in ents if isinstance(e, NumberInfo)}
    selects = {e.name: e for e in ents if isinstance(e, SelectInfo)}
    sensors = {e.name: e for e in ents if isinstance(e, SensorInfo)}

    def on_state(s) -> None:
        e = by_key.get(s.key)
        if e is None or "LD2450" not in e.name:
            return
        if isinstance(s, NumberState):
            print(
                f"{stamp()} NUM {e.name} = {s.state:.0f}{' (missing)' if s.missing_state else ''}"
            )
        elif isinstance(s, SelectState):
            print(f"{stamp()} SEL {e.name} = {s.state}")
        elif isinstance(s, SensorState) and "Count" in e.name:
            print(f"{stamp()} CNT {e.name} = {s.state:.0f}")

    c.subscribe_states(on_state)
    await asyncio.sleep(1.5)
    for op in OPS:
        if op == "list":
            for n in sorted(numbers):
                print("number:", n)
            for n in sorted(selects):
                print("select:", n, selects[n].options)
            for n in sorted(sensors):
                if "Count" in n:
                    print("sensor:", n)
            continue
        kind, _, rest = op.partition(":")
        if kind == "wait":
            await asyncio.sleep(float(rest))
            continue
        name, _, value = rest.partition("=")
        if kind == "set":
            if name not in numbers:
                sys.exit(f"no number named {name!r}; have {sorted(numbers)}")
            print(f"{stamp()} >>> set {name} = {value}")
            c.number_command(numbers[name].key, float(value))
        elif kind == "sel":
            if name not in selects:
                sys.exit(f"no select named {name!r}; have {sorted(selects)}")
            print(f"{stamp()} >>> select {name} = {value}")
            c.select_command(selects[name].key, value)
        else:
            sys.exit(f"unknown op {op!r}")
        await asyncio.sleep(0.3)
    await c.disconnect()


asyncio.run(main())
