"""Drive the SEN0395 test device over the API and print its binary sensor and switch updates.
usage: ctl.py HOST KEY OP [OP ...]   OP: press:NAME | sw:NAME=on|off | wait:SECONDS
"""

import asyncio
import sys
import time

from aioesphomeapi import (
    APIClient,
    BinarySensorState,
    ButtonInfo,
    SwitchInfo,
    SwitchState,
)

HOST, KEY = sys.argv[1], sys.argv[2]
OPS = sys.argv[3:]
T0 = time.time()


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=KEY)
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    by_key = {e.key: e for e in ents}
    buttons = {e.name: e for e in ents if isinstance(e, ButtonInfo)}
    switches = {e.name: e for e in ents if isinstance(e, SwitchInfo)}

    def on_state(s) -> None:
        e = by_key.get(s.key)
        if isinstance(s, BinarySensorState):
            print(
                f"{time.time() - T0:7.3f} BIN {e.name} = {'ON' if s.state else 'off'}"
            )
        elif isinstance(s, SwitchState):
            print(
                f"{time.time() - T0:7.3f} SW  {e.name} = {'on' if s.state else 'off'}"
            )

    c.subscribe_states(on_state)
    await asyncio.sleep(1.0)
    for op in OPS:
        kind, _, rest = op.partition(":")
        if kind == "wait":
            await asyncio.sleep(float(rest))
        elif kind == "press":
            if rest not in buttons:
                sys.exit(f"no button {rest!r}; have {sorted(buttons)}")
            print(f"{time.time() - T0:7.3f} >>> press {rest}")
            c.button_command(buttons[rest].key)
        elif kind == "sw":
            name, _, val = rest.partition("=")
            if name not in switches:
                sys.exit(f"no switch {name!r}; have {sorted(switches)}")
            print(f"{time.time() - T0:7.3f} >>> switch {name} {val}")
            c.switch_command(switches[name].key, val == "on")
        else:
            sys.exit(f"unknown op {op!r}")
    await c.disconnect()


asyncio.run(main())
