"""Read or drive a bulb directly over its own ESPHome API (an HA-side change, independent of the mirror).

usage: bulb.py HOST KEY state | on [brightness 0-1] [ct_mireds|rgb r,g,b] | off | restart
"""

import asyncio
import sys

from aioesphomeapi import APIClient, ButtonInfo, LightColorCapability, LightInfo

HOST, KEY, CMD = sys.argv[1], sys.argv[2], sys.argv[3]
ARGS = sys.argv[4:]


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=None if KEY == "-" else KEY)
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    light = next((e for e in ents if isinstance(e, LightInfo)), None)
    if light is None:
        sys.exit("no light entity on this device; is HOST a bulb?")
    if CMD == "state":
        fut = asyncio.get_running_loop().create_future()

        def on_state(st):
            if getattr(st, "key", None) == light.key and not fut.done():
                fut.set_result(st)

        c.subscribe_states(on_state)
        st = await asyncio.wait_for(fut, 5)
        print(
            f"{light.name}: on={st.state} mode={st.color_mode} bri={st.brightness:.2f} "
            f"ct={st.color_temperature:.1f} rgb={st.red:.2f}/{st.green:.2f}/{st.blue:.2f} "
            f"white={st.white:.2f} cold={st.cold_white:.2f} warm={st.warm_white:.2f}"
        )
    elif CMD == "on":
        kw = {"state": True}
        if ARGS:
            kw["brightness"] = float(ARGS[0])
        if len(ARGS) > 1:
            if "," in ARGS[1]:
                r, g, b = (float(x) for x in ARGS[1].split(","))
                kw["rgb"] = (r, g, b)
                kw["color_mode"] = (
                    LightColorCapability.RGB
                    | LightColorCapability.ON_OFF
                    | LightColorCapability.BRIGHTNESS
                )
            else:
                kw["color_temperature"] = float(ARGS[1])
                kw["color_mode"] = (
                    LightColorCapability.COLOR_TEMPERATURE
                    | LightColorCapability.ON_OFF
                    | LightColorCapability.BRIGHTNESS
                )
        c.light_command(light.key, **kw)
        print("bulb on", kw)
    elif CMD == "off":
        c.light_command(light.key, state=False)
        print("bulb off")
    elif CMD == "restart":
        btn = next(
            (
                e
                for e in ents
                if isinstance(e, ButtonInfo) and "restart" in e.name.lower()
            ),
            None,
        )
        if btn is None:
            sys.exit("no restart button on this device")
        c.button_command(btn.key)
        print("bulb restart pressed")
    await asyncio.sleep(0.5)
    await c.disconnect()


asyncio.run(main())
