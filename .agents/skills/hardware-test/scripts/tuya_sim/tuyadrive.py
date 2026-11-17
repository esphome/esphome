"""Drive the tuyasim scenario over the API and record every entity state (esphome#22724).

usage: tuyadrive.py HOST OUT.json   (API_KEY in the env)
Presses "Sim start", waits for the scripted datapoint changes, then sends switch, select, water heater, number and
fan commands. OUT.json gets the ordered state list per entity; a summary is printed.
"""

import asyncio
from collections import Counter
import json
import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from aioesphomeapi import WaterHeaterMode  # noqa: E402
from apiconn import connect  # noqa: E402

STEPS_WAIT = 32  # 18 steps 1.5 s apart, plus margin


def val(st):
    d = {}
    for k in (
        "state",
        "missing_state",
        "current_temperature",
        "target_temperature",
        "mode",
        "oscillating",
        "speed_level",
    ):
        if hasattr(st, k):
            x = getattr(st, k)
            d[k] = x.value if hasattr(x, "value") else x
    return d


async def main(host: str, out: str) -> None:
    c = await connect(host)
    ents, _ = await c.list_entities_services()
    by_key = {e.key: e for e in ents}
    by_id = {e.object_id: e for e in ents}
    events = []
    phase = ["initial"]
    t0 = time.monotonic()

    def on_state(st):
        e = by_key.get(st.key)
        events.append(
            {
                "t": round(time.monotonic() - t0, 3),
                "phase": phase[0],
                "entity": e.object_id if e else st.key,
                "value": val(st),
            }
        )

    c.subscribe_states(on_state)
    await asyncio.sleep(3)
    phase[0] = "scenario"
    c.button_command(by_id["sim_start"].key)
    await asyncio.sleep(STEPS_WAIT)

    phase[0] = "commands"
    k = {n: by_id[n].key for n in by_id}
    cmds = [
        ("sw on", lambda: c.switch_command(k["sw_dp1"], True)),
        ("sw off", lambda: c.switch_command(k["sw_dp1"], False)),
        ("sel_enum Two", lambda: c.select_command(k["sel_enum_dp4"], "Two")),
        ("sel_str manual", lambda: c.select_command(k["sel_string_dp24"], "manual")),
        ("sel_int High", lambda: c.select_command(k["sel_int_dp25"], "High")),
        ("wh on", lambda: c.water_heater_command(k["wh"], on=True)),
        (
            "wh target 55",
            lambda: c.water_heater_command(k["wh"], target_temperature=55),
        ),
        (
            "wh mode eco",
            lambda: c.water_heater_command(k["wh"], mode=WaterHeaterMode.ECO),
        ),
        (
            "wh mode electric",
            lambda: c.water_heater_command(k["wh"], mode=WaterHeaterMode.ELECTRIC),
        ),
        ("wh off", lambda: c.water_heater_command(k["wh"], on=False)),
        ("num 3", lambda: c.number_command(k["num_dp30"], 3)),
        ("fan on", lambda: c.fan_command(k["fan_dp31"], state=True)),
        ("fan off", lambda: c.fan_command(k["fan_dp31"], state=False)),
    ]
    for name, fn in cmds:
        events.append(
            {
                "t": round(time.monotonic() - t0, 3),
                "phase": "cmd",
                "entity": "CMD",
                "value": name,
            }
        )
        fn()
        await asyncio.sleep(2)
    await asyncio.sleep(3)
    await c.disconnect()

    with pathlib.Path(out).open("w", encoding="utf-8") as f:
        json.dump(events, f, indent=1)
    counts = Counter(
        (ev["phase"], ev["entity"]) for ev in events if ev["entity"] != "CMD"
    )
    for (ph, ent), n in sorted(counts.items()):
        print(f"{ph:9} {ent:20} {n}")
    print("TOTAL", sum(counts.values()))


if __name__ == "__main__":
    asyncio.run(main(sys.argv[1], sys.argv[2]))
