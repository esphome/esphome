"""drive.py CLIENT_IP FAKE_IP [faults|control|midbatch]...: daikin_madoka client against the fake BRC1H.

faults: per fault, a unique cooling setpoint and alternating COOL/HEAT through the client, two polls, then the client
state next to the fake's STATE line. control: modes, setpoints with clamping, fan speeds. midbatch: a setpoint SET
fired when the fake sees the poll's first GET (and, with tail_late, when a setpoint tail is deferred); prints the
fake's RX/TX order around it and whether the SET and all five GET replies went through.
Exits non-zero when a midbatch SET did not read back.
"""

import asyncio
import re
import sys
import time

from aioesphomeapi import (
    APIClient,
    ClimateFanMode,
    ClimateInfo,
    ClimateMode,
    LogLevel,
    SelectInfo,
)

FAULTS = [
    "none",
    "drop_sensor",
    "none",
    "ff_record",
    "none",
    "out_of_order",
    "none",
    "tail_late",
    "none",
    "drop_setpoint",
    "none",
    "delay_100",
    "delay_300",
    "slow_1s",
    "none",
]
CONTROL = [
    ("mode HEAT", {"mode": ClimateMode.HEAT}),
    ("mode HEAT_COOL", {"mode": ClimateMode.HEAT_COOL}),
    ("mode FAN_ONLY", {"mode": ClimateMode.FAN_ONLY}),
    ("mode DRY", {"mode": ClimateMode.DRY}),
    ("mode OFF", {"mode": ClimateMode.OFF}),
    ("mode COOL", {"mode": ClimateMode.COOL}),
    (
        "setpoints 18.5/26.5",
        {"target_temperature_low": 18.5, "target_temperature_high": 26.5},
    ),
    (
        "setpoints 10/40 (clamp)",
        {"target_temperature_low": 10.0, "target_temperature_high": 40.0},
    ),
    ("fan LOW", {"fan_mode": ClimateFanMode.LOW}),
    ("fan HIGH", {"fan_mode": ClimateFanMode.HIGH}),
    ("fan AUTO", {"fan_mode": ClimateFanMode.AUTO}),
    ("fan MEDIUM", {"fan_mode": ClimateFanMode.MEDIUM}),
]
MIDBATCH = [
    ("none", "RX 06 00 00 20"),
    ("none", "RX 06 00 00 40"),
    ("tail_late", "RX 06 00 00 20"),
    ("tail_late", "TAIL"),
    ("tail_late", "RX 06 00 00 50"),
] * 2


async def main() -> int:
    cli_ip, fake_ip = sys.argv[1], sys.argv[2]
    phases = sys.argv[3:] or ["faults", "control", "midbatch"]
    cli = APIClient(cli_ip, 6053, None)
    await cli.connect(login=True)
    fake = APIClient(fake_ip, 6053, None)
    await fake.connect(login=True)
    ci, _ = await cli.list_entities_services()
    fi, _ = await fake.list_entities_services()
    ckey = next(i.key for i in ci if isinstance(i, ClimateInfo))
    fsel = next(i.key for i in fi if isinstance(i, SelectInfo))
    st, pubs, fake_state, flog, hook = {}, [], {}, [], {}

    def on_state(s) -> None:
        if s.key == ckey:
            st["s"] = s
            pubs.append(time.time())

    cli.subscribe_states(on_state)

    def on_flog(m) -> None:
        t = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
        if "STATE " in t:
            fake_state["s"] = t.split("STATE ", 1)[1]
        for k in (" RX ", " TX ", "TAIL", "HOLD", "DROP"):
            if k in t:
                flog.append((time.time(), t.split("]: ", 1)[-1]))
                if hook.get("fn"):
                    hook["fn"](t)
                break

    fake.subscribe_logs(on_flog, log_level=LogLevel.LOG_LEVEL_INFO)

    def cstate() -> str:
        s = st.get("s")
        if s is None:
            return "no state"
        fan = s.fan_mode.name if s.fan_mode is not None else None
        return (
            f"mode={s.mode.name} low={s.target_temperature_low} high={s.target_temperature_high}"
            f" cur={s.current_temperature} fan={fan}"
        )

    def show(label: str) -> None:
        print(
            f"{label:38} client: {cstate()} | fake: {fake_state.get('s')} | publishes {len(pubs)}",
            flush=True,
        )

    await asyncio.sleep(12)
    show("baseline")
    vals = iter([x / 2 for x in range(42, 70)])
    modes = iter([ClimateMode.HEAT, ClimateMode.COOL] * 12)
    bad = 0
    if "faults" in phases:
        for fault in FAULTS:
            fake.select_command(fsel, fault)
            await asyncio.sleep(2)
            sp = next(vals)
            m = next(modes)
            pubs.clear()
            cli.climate_command(
                ckey, mode=m, target_temperature_low=19.0, target_temperature_high=sp
            )
            await asyncio.sleep(24)
            show(f"fault {fault}, set {m.name} high={sp}")
        fake.select_command(fsel, "none")
    if "control" in phases:
        for label, kw in CONTROL:
            pubs.clear()
            cli.climate_command(ckey, **kw)
            await asyncio.sleep(14)
            show(label)
    if "midbatch" in phases:
        cli.climate_command(ckey, mode=ClimateMode.COOL, target_temperature_low=19.0)
        await asyncio.sleep(12)
        for fault, trigger in MIDBATCH:
            fake.select_command(fsel, fault)
            await asyncio.sleep(12)
            sp = next(vals)
            fired = {}

            def fn(
                t: str, sp: float = sp, trigger: str = trigger, fired: dict = fired
            ) -> None:
                if not fired and trigger in t:
                    fired["t"] = time.time()
                    cli.climate_command(ckey, target_temperature_high=sp)

            flog.clear()
            pubs.clear()
            hook["fn"] = fn
            await asyncio.sleep(12)
            hook["fn"] = None
            t0 = fired.get("t")
            win = [(t, line) for t, line in flog if t0 and t0 - 0.05 <= t <= t0 + 1.5]
            cmds = [line.split()[1:5] for _, line in win if line.startswith("RX ")]
            rx = [c[3] if len(c) > 3 else "?" for c in cmds]
            tx = [line for _, line in win if line.startswith("TX ")]
            await asyncio.sleep(12)
            s = st.get("s")
            ok = (
                t0 is not None
                and s is not None
                and abs(s.target_temperature_high - sp) < 0.01
            )
            bad += not ok
            print(
                f"midbatch fault={fault} trigger='{trigger}' set high={sp}: fired={'yes' if t0 else 'no'} "
                f"rx order {' '.join(rx)} | tx {len(tx)} | client high={s.target_temperature_high if s else None} "
                f"{'OK' if ok else 'MISMATCH'} | fake: {fake_state.get('s')}",
                flush=True,
            )
            for t, line in win:
                print(f"    +{(t - t0) * 1000:5.0f} ms {line[:90]}", flush=True)
        fake.select_command(fsel, "none")
    await cli.disconnect()
    await fake.disconnect()
    if "midbatch" in phases:
        print(f"midbatch: {bad} mismatches", flush=True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
