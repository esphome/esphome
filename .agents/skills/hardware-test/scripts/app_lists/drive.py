"""List and drive every entity of entities.yaml over the API, for A/B checks of entity registration changes.
Usage: drive.py HOST OUTPREFIX [drive]   env: API_KEY (see apiconn.py), NOLOGS=1 skips the dump_config log
Writes OUT-list.json (device_info with devices and areas, every entity sorted by type, device and object_id) and
OUT-dump.txt (the dump_config log), waits up to 20 s for a state of every stateful entity, and with `drive` sends
15 commands (switch on/off, button, light on/off, number, select, fan, cover, text, lock, valve, date, time,
datetime), each waiting for its state. Diff OUT-list.json between builds with the device name masked."""

import asyncio
import contextlib
import json
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from aioesphomeapi import LightState, LockCommand, LogLevel, SwitchState  # noqa: E402
from apiconn import connect, log_text  # noqa: E402

NO_STATE = ("ButtonInfo", "EventInfo")


async def main() -> None:
    host, out = sys.argv[1], sys.argv[2]
    drive = "drive" in sys.argv[3:]
    cli = await connect(host)
    di = await cli.device_info()
    ents, _ = await cli.list_entities_services()
    rows = []
    for e in ents:
        d = e.to_dict()
        d["_type"] = type(e).__name__
        rows.append(d)
    rows.sort(key=lambda d: (d["_type"], d.get("device_id", 0), d["object_id"]))
    info = {
        "name": di.name,
        "friendly_name": di.friendly_name,
        "suggested_area": di.suggested_area,
        "area": di.area.to_dict() if di.area else None,
        "devices": [d.to_dict() for d in di.devices],
        "areas": [a.to_dict() for a in di.areas],
    }
    Path(f"{out}-list.json").write_text(
        json.dumps({"device_info": info, "entities": rows}, indent=1, sort_keys=True),
        encoding="utf-8",
    )
    print(
        f"entities={len(rows)} devices={len(di.devices)} areas={len(di.areas)} area={info['area']}"
    )
    for d in rows:
        print(
            f"  {d['_type']:<22} dev={d.get('device_id', 0):<10} key={d['key']:<10} "
            f"oid={d['object_id']!r} name={d['name']!r}"
        )

    logs: list[str] = []
    if not os.environ.get("NOLOGS"):
        cli.subscribe_logs(
            lambda m: logs.append(log_text(m)),
            log_level=LogLevel.LOG_LEVEL_CONFIG,
            dump_config=True,
        )
    states: dict[tuple[int, int], object] = {}
    changed = asyncio.Event()
    loop = asyncio.get_running_loop()

    def on_state(s) -> None:
        states[(s.key, getattr(s, "device_id", 0))] = s
        changed.set()

    want_keys = {
        (d["key"], d.get("device_id", 0)) for d in rows if d["_type"] not in NO_STATE
    }
    cli.subscribe_states(on_state)
    end = loop.time() + 20
    while not want_keys <= states.keys() and (rem := end - loop.time()) > 0:
        changed.clear()
        with contextlib.suppress(TimeoutError):
            await asyncio.wait_for(changed.wait(), rem)

    async def wait_for(e, check, t: float = 15.0) -> bool:
        # drop the cached state so only an update sent after the command counts
        states.pop((e.key, e.device_id), None)
        end = loop.time() + t
        while True:
            if (s := states.get((e.key, e.device_id))) is not None and check(s):
                return True
            if (rem := end - loop.time()) <= 0:
                return False
            changed.clear()
            with contextlib.suppress(TimeoutError):
                await asyncio.wait_for(changed.wait(), rem)

    def ent(cls_name: str, name: str, sub: bool | None = None):
        return next(
            e
            for e in ents
            if type(e).__name__ == cls_name
            and e.name == name
            and (sub is None or bool(e.device_id) == sub)
        )

    if drive:
        relay = ent("SwitchInfo", "Relay", sub=False)
        relay2 = ent("SwitchInfo", "Relay", sub=True)
        lamp = ent("LightInfo", "Lamp")
        btn = ent("ButtonInfo", "Press Me")
        r2_before = getattr(states.get((relay2.key, relay2.device_id)), "state", None)
        for want in (True, False):
            cli.switch_command(relay.key, want)
            ok = await wait_for(
                relay, lambda s, w=want: isinstance(s, SwitchState) and s.state == w
            )
            print(f"DRIVE switch Relay(main) -> {want}: {'OK' if ok else 'NO STATE'}")
        cli.button_command(btn.key)
        if r2_before is None:
            print("DRIVE button Press Me: NO BASELINE (sub device Relay had no state)")
        else:
            ok = await wait_for(relay2, lambda s: s.state == (not r2_before))
            print(
                f"DRIVE button Press Me -> sub device Relay toggled from {r2_before}: {'OK' if ok else 'NO STATE'}"
            )
        for want in (True, False):
            cli.light_command(lamp.key, state=want, brightness=0.5 if want else None)
            ok = await wait_for(
                lamp, lambda s, w=want: isinstance(s, LightState) and s.state == w
            )
            print(f"DRIVE light Lamp -> {want}: {'OK' if ok else 'NO STATE'}")
        # (entity type, name, command method, args, keyword args, expected state fields)
        pos = {"position": 1.0}
        cases = [
            ("NumberInfo", "Level", "number_command", (42.0,), {}, {"state": 42.0}),
            ("SelectInfo", "Mode", "select_command", ("b",), {}, {"state": "b"}),
            ("CoverInfo", "Shade", "cover_command", (), pos, pos),
            ("TextInfo", "Note", "text_command", ("hello",), {}, {"state": "hello"}),
            ("LockInfo", "Latch", "lock_command", (LockCommand.UNLOCK,), {}, {"state": 2}),  # lock dedups, so toggle
            ("LockInfo", "Latch", "lock_command", (LockCommand.LOCK,), {}, {"state": 1}),
            ("ValveInfo", "Tap", "valve_command", (), pos, pos),
            ("DateInfo", "Day", "date_command", (2026, 11, 24), {}, {"day": 24}),
            ("TimeInfo", "Clock", "time_command", (12, 34, 56), {}, {"second": 56}),
            ("DateTimeInfo", "Stamp", "datetime_command", (1764000000,), {}, {"epoch_seconds": 1764000000}),
        ]  # fmt: skip
        fan = ent("FanInfo", "Fan")
        cli.fan_command(fan.key, state=True, speed_level=2, device_id=fan.device_id)
        ok = await wait_for(fan, lambda s: s.state and s.speed_level == 2)
        print(f"DRIVE fan Fan on speed 2: {'OK' if ok else 'NO STATE'}")
        for cls_name, name, method, args, kw, want in cases:
            e = ent(cls_name, name)
            getattr(cli, method)(e.key, *args, device_id=e.device_id, **kw)
            ok = await wait_for(
                e, lambda s, w=want: all(getattr(s, k) == v for k, v in w.items())
            )
            print(f"DRIVE {method} {name} {args or ''}: {'OK' if ok else 'NO STATE'}")
        await asyncio.sleep(2)

    missing = [
        f"{d['_type']}:{d['name']}@{d.get('device_id', 0)}"
        for d in rows
        if (d["key"], d.get("device_id", 0)) not in states
        and d["_type"] not in NO_STATE
    ]
    print(f"states for {len(states)} keys; entities without a state: {missing}")
    for k, s in sorted(states.items()):
        if "Sensor" in type(s).__name__:
            print(
                f"  STATE {type(s).__name__} key={k[0]} dev={k[1]} {getattr(s, 'state', None)!r}"
            )
    # dump_config logs one component per loop pass: save after 3 s without a new line, 30 s at most
    end, n = loop.time() + 30, -1
    while not os.environ.get("NOLOGS") and n != len(logs) and loop.time() < end:
        n = len(logs)
        await asyncio.sleep(3)
    Path(f"{out}-dump.txt").write_text("\n".join(logs) + "\n", encoding="utf-8")
    print(f"dump_config lines: {len(logs)}")
    await cli.disconnect()


asyncio.run(main())
