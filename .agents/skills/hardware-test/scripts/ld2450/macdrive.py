"""Read the LD24xx BT MAC text sensors, turn every LD24xx Bluetooth switch off, then on again.

usage: macdrive.py HOST OUT   env: API_KEY (when the device has one)
Works on any device whose MAC, Bluetooth and firmware entities have those words in their names (the BT MAC sensors
may be disabled by default; they still publish over the API). Quote at most the first MAC byte in a PR.
"""

import asyncio
import os
import pathlib
import sys
import time

from aioesphomeapi import APIClient, SwitchInfo, TextSensorInfo

HOST, OUT = sys.argv[1], sys.argv[2]
LOG = pathlib.Path(OUT).open("w", encoding="utf-8")  # noqa: SIM115  open for the whole run


def note(s: str) -> None:
    line = f"{time.strftime('%H:%M:%S')} {s}"
    print(line)
    LOG.write(line + "\n")
    LOG.flush()


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=os.environ.get("API_KEY"))
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    keys = {
        e.key: e.name
        for e in ents
        if isinstance(e, (SwitchInfo, TextSensorInfo))
        and any(w in e.name for w in ("MAC", "Bluetooth", "Firmware"))
    }
    states = {}

    def on_state(s) -> None:
        if s.key in keys and states.get(keys[s.key]) != getattr(s, "state", None):
            states[keys[s.key]] = s.state
            note(f"STATE {keys[s.key]} = {s.state}")

    c.subscribe_states(on_state)
    await asyncio.sleep(8)
    switches = {
        e.name: e.key
        for e in ents
        if isinstance(e, SwitchInfo) and "Bluetooth" in e.name
    }
    macs = [n for n in keys.values() if "MAC" in n]
    if not macs or not switches:
        note(
            f"FAIL: need a MAC text sensor and a Bluetooth switch, found {sorted(keys.values())}"
        )
        sys.exit(1)
    missing = []
    if silent := [n for n in sorted(keys.values()) if n not in states]:
        note(f"NO-DATA after 8 s: {', '.join(silent)}")
        missing += silent
    note(
        "SNAP start: "
        + "; ".join(f"{n}={states.get(n)}" for n in sorted(keys.values()))
    )
    for phase, on in (("off", False), ("on", True)):
        for name, key in switches.items():
            note(f"SET {name} {phase}")
            c.switch_command(key, on)
            await asyncio.sleep(1)
        await asyncio.sleep(20)
        note(
            f"SNAP bluetooth {phase}: "
            + "; ".join(f"{n}={states.get(n)}" for n in sorted(keys.values()))
        )
    missing += [n for n in switches if states.get(n) is not True]
    await c.disconnect()
    if missing:
        note(f"NO-DATA for: {', '.join(sorted(set(missing)))}")
        sys.exit(1)


asyncio.run(main())
