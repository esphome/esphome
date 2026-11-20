"""drive.py IP STEP...: STEP is 'Name:mode=cool,t=24,fan=low,swing=vertical' or 'svc:action'; prints climate states after each."""

import asyncio
import os
import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from aioesphomeapi import (
    ClimateFanMode,
    ClimateInfo,
    ClimateMode,
    ClimatePreset,
    ClimateSwingMode,
)
from apiconn import connect

MODES = {m.name.lower(): m for m in ClimateMode}
FANS = {m.name.lower(): m for m in ClimateFanMode}
SWINGS = {m.name.lower(): m for m in ClimateSwingMode}
PRESETS = {m.name.lower(): m for m in ClimatePreset}


async def main() -> None:
    c = await connect(sys.argv[1])
    ents, svcs = await c.list_entities_services()
    clim = {e.name: e for e in ents if isinstance(e, ClimateInfo)}
    byk = {e.key: e.name for e in clim.values()}
    st = {}
    c.subscribe_states(
        lambda s: st.__setitem__(byk[s.key], s) if s.key in byk else None
    )
    await asyncio.sleep(2)

    def show(tag):
        for n in sorted(st):
            s = st[n]
            print(
                f"  {tag} {n:11s} mode={s.mode.name} target={s.target_temperature:g} fan={s.fan_mode.name} swing={s.swing_mode.name} preset={s.preset.name}"
            )

    show("init")
    for step in sys.argv[2:]:
        name, _, args = step.partition(":")
        print(time.strftime("%H:%M:%S"), "STEP", step, flush=True)
        if name == "svc":
            sname, _, sargs = args.partition("?")
            data = {
                k: v == "1"
                for k, v in (a.split("=") for a in filter(None, sargs.split("&")))
            }
            await c.execute_service(next(s for s in svcs if s.name == sname), data)
        else:
            kw = {}
            for a in filter(None, args.split(",")):
                k, v = a.split("=")
                if k == "mode":
                    kw["mode"] = MODES[v]
                elif k == "t":
                    kw["target_temperature"] = float(v)
                elif k == "fan":
                    kw["fan_mode"] = FANS[v]
                elif k == "swing":
                    kw["swing_mode"] = SWINGS[v]
                elif k == "preset":
                    kw["preset"] = PRESETS[v]
            c.climate_command(clim[name].key, **kw)
        await asyncio.sleep(float(os.environ.get("STEP_S", "2.5")))
        show("after")
    await c.disconnect()


asyncio.run(main())
