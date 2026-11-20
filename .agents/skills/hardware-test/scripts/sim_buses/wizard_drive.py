"""wizard_drive.py HOST: act as Home Assistant finishing the device wizard, then check the inputs reached the entities.

Needs aioesphomeapi 46.28 or later (device_wizard, wizard_input_set) and the esphome checkout on PYTHONPATH (for
fnv1_hash). The DUT logs "WZ ..." lines from a "Dump" button. Exits 1 unless every input arrived.
"""

import asyncio
import re
import sys

from aioesphomeapi import APIClient, ButtonInfo, LogLevel

from esphome.helpers import fnv1_hash

HOST = sys.argv[1]
SET = {
    "outdoor_temp": "sensor.outdoor_temperature",
    "house_mode": "input_select.house_mode_x",
    "weather_input": "weather.home_forecast",
    "calendars_input": [
        "calendar.work",
        "calendar.family",
        "calendar.holidays_in_some_country",
    ],
}
STATES = {"sensor.outdoor_temperature": "21.5", "input_select.house_mode_x": "Vacation"}
WANT = (
    "WZ outdoor value 21.5",
    "WZ house_mode value Vacation",
    "WZ weather set=1 id=weather.home_forecast",
    "WZ calendars set=1 count=3",
    "WZ calendar calendar.holidays_in_some_country",
)
ANSI = re.compile(r"\x1b\[[0-9;]*m")


async def main() -> int:
    lines: list[str] = []
    c = APIClient(HOST, 6053, None)
    await c.connect(login=True)
    wiz = await c.device_wizard()
    print("wizard pages:", wiz.pages)
    for name, val in SET.items():
        c.wizard_input_set(fnv1_hash(name), val)
    ents, _ = await c.list_entities_services()
    dump = next(e.key for e in ents if isinstance(e, ButtonInfo) and e.name == "Dump")
    c.subscribe_logs(
        lambda m: lines.append(ANSI.sub("", m.message.decode(errors="replace"))),
        log_level=LogLevel.LOG_LEVEL_DEBUG,
    )
    subs = []

    def on_sub(entity_id: str, attribute: str | None) -> None:
        subs.append((entity_id, attribute or None))
        if entity_id in STATES:
            c.send_home_assistant_state(entity_id, attribute or None, STATES[entity_id])

    c.subscribe_home_assistant_states(on_sub)
    await asyncio.sleep(2)
    c.button_command(dump)
    await asyncio.sleep(2)
    print("subscriptions:", subs)
    for line in lines:
        if "WZ " in line:
            print("  ", line[line.find("WZ ") :])
    ok = all((e, None) in subs for e in STATES) and all(
        any(w in line for line in lines) for w in WANT
    )
    print("RESULT", "PASS" if ok else "FAIL")
    await c.disconnect()
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
