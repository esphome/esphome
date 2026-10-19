"""Held-report cases for esphome#21144 with persistent API connections, so the steps land inside the 3 s fade.
usage: heldcase.py DEV_IP KEY BULB_IP same|many
  t0      press "A 50 fade 3s" on the mirror device (3 s fade to 50%)
  t0+0.7  set the bulb directly to 90% (Home Assistant reports it to the mirror during the fade: held)
  t0+1.4  same:  press "A 50 fade 3s" again (same target as the running fade)
          many:  press "A many calls" (16 calls in one loop pass, the last one 100% with a 1 s fade)
  t0+6.5  read the bulb, press Log state so the mirror prints its state
"""

import asyncio
import sys
import time

from aioesphomeapi import APIClient, ButtonInfo, LightInfo, LightState

DEV, KEY, BULB, CASE = sys.argv[1:5]
T0 = time.time()


def stamp() -> str:
    return f"{time.time() - T0:6.3f}"


async def main() -> None:
    dev = APIClient(DEV, 6053, None, noise_psk=KEY)
    bulb = APIClient(BULB, 6053, None, noise_psk=None)
    await dev.connect(login=True)
    await bulb.connect(login=True)
    dents, _ = await dev.list_entities_services()
    bents, _ = await bulb.list_entities_services()
    buttons = {e.name: e for e in dents if isinstance(e, ButtonInfo)}
    blight = next(e for e in bents if isinstance(e, LightInfo))
    states = {}

    def on_bulb(s) -> None:
        if isinstance(s, LightState) and s.key == blight.key:
            states["bulb"] = s
            print(f"{stamp()} bulb state: on={s.state} bri={s.brightness:.2f}")

    bulb.subscribe_states(on_bulb)
    await asyncio.sleep(1.0)
    print(f"{stamp()} >>> bulb to 100% (precondition)")
    bulb.light_command(blight.key, state=True, brightness=1.0)
    await asyncio.sleep(4.0)
    t = time.time()
    print(f"{stamp()} >>> t0 press A 50 fade 3s")
    dev.button_command(buttons["A 50 fade 3s"].key)
    await asyncio.sleep(0.7 - (time.time() - t))
    print(f"{stamp()} >>> bulb direct to 90%")
    bulb.light_command(blight.key, state=True, brightness=0.9)
    await asyncio.sleep(1.4 - (time.time() - t))
    name = "A 50 fade 3s" if CASE == "same" else "A many calls"
    print(f"{stamp()} >>> press {name}")
    dev.button_command(buttons[name].key)
    await asyncio.sleep(6.5 - (time.time() - t))
    print(f"{stamp()} >>> press Log state (the mirror's state is in the device log)")
    dev.button_command(buttons["Log state"].key)
    await asyncio.sleep(1.0)
    b = states.get("bulb")
    print(f"{stamp()} RESULT bulb bri={b.brightness:.2f}")
    await dev.disconnect()
    await bulb.disconnect()


asyncio.run(main())
