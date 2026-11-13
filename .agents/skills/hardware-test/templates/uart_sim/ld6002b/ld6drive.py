"""ld6drive.py HOST [...]: press every button, set every number and select, toggle every switch; print states.

Further arguments are ignored, so scripts/uart_sim/run.sh can call it with DRIVER=ld6drive.py.
"""

import asyncio
import math
import sys
import time

from aioesphomeapi import APIClient, ButtonInfo, NumberInfo, SelectInfo, SwitchInfo

T0 = time.time()


async def main() -> None:
    cli = APIClient(sys.argv[1], 6053, None)
    await cli.connect(login=True)
    infos, _ = await cli.list_entities_services()
    names = {i.key: i.name for i in infos}
    states = {}

    def on_state(s):
        v = getattr(s, "state", None)
        if isinstance(v, float):
            v = "nan" if math.isnan(v) else round(v, 3)
        states[names.get(s.key, s.key)] = v
        print(
            f"{time.time() - T0:7.2f} STATE {names.get(s.key, s.key)} = {v}", flush=True
        )

    cli.subscribe_states(on_state)
    await asyncio.sleep(12)
    print("ACTIONS", flush=True)
    for i in sorted(
        (x for x in infos if isinstance(x, ButtonInfo)), key=lambda x: x.name
    ):
        print(f"{time.time() - T0:7.2f} DO press {i.name}", flush=True)
        cli.button_command(i.key)
        await asyncio.sleep(0.8)
    for i in sorted(
        (x for x in infos if isinstance(x, NumberInfo)), key=lambda x: x.name
    ):
        v = i.min_value + (i.max_value - i.min_value) * 0.37
        if i.step:
            v = round(round(v / i.step) * i.step, 3)
        print(f"{time.time() - T0:7.2f} DO number {i.name} = {v}", flush=True)
        cli.number_command(i.key, v)
        await asyncio.sleep(0.8)
    for i in sorted(
        (x for x in infos if isinstance(x, SelectInfo)), key=lambda x: x.name
    ):
        opt = i.options[1] if len(i.options) > 1 else i.options[0]
        print(f"{time.time() - T0:7.2f} DO select {i.name} = {opt}", flush=True)
        cli.select_command(i.key, opt)
        await asyncio.sleep(0.8)
    for i in sorted(
        (x for x in infos if isinstance(x, SwitchInfo)), key=lambda x: x.name
    ):
        for on in (True, False):
            print(f"{time.time() - T0:7.2f} DO switch {i.name} = {on}", flush=True)
            cli.switch_command(i.key, on)
            await asyncio.sleep(0.8)
    await asyncio.sleep(2)
    print("FINAL", flush=True)
    for k in sorted(states):
        print(f"FINAL {k} = {states[k]}", flush=True)
    await cli.disconnect()


asyncio.run(main())
