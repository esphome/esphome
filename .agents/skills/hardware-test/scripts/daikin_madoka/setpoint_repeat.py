"""setpoint_repeat.py CLIENT_IP FAKE_IP [x]: setpoint pair changes in COOL with a timeline of fake RX/TX and publishes.

With a third argument every change is run twice and preceded by a mode command 14 s earlier (the drive.py control
order). Exits non-zero when a readback does not match the setpoints sent.
"""

import asyncio
import re
import sys
import time

from aioesphomeapi import APIClient, ClimateInfo, ClimateMode, LogLevel


async def main() -> int:
    cli = APIClient(sys.argv[1], 6053, None)
    await cli.connect(login=True)
    fake = APIClient(sys.argv[2], 6053, None)
    await fake.connect(login=True)
    with_mode = len(sys.argv) > 3
    ci, _ = await cli.list_entities_services()
    ckey = next(i.key for i in ci if isinstance(i, ClimateInfo))
    t0 = [time.time()]
    st = {}

    def on_state(s) -> None:
        if s.key == ckey:
            st["s"] = s
            print(
                f"  +{time.time() - t0[0]:6.2f} PUB mode={s.mode.name} low={s.target_temperature_low}"
                f" high={s.target_temperature_high}",
                flush=True,
            )

    cli.subscribe_states(on_state)

    def on_flog(m) -> None:
        t = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
        if " RX " in t and (" 40 40 " in t or "00 00 40 00" in t):
            print(
                f"  +{time.time() - t0[0]:6.2f} fake {t.split(']: ', 1)[-1][:60]}",
                flush=True,
            )
        if " TX " in t and "00 00 40 20" in t:
            print(
                f"  +{time.time() - t0[0]:6.2f} fake {t.split(']: ', 1)[-1][:40]}",
                flush=True,
            )

    fake.subscribe_logs(on_flog, log_level=LogLevel.LOG_LEVEL_INFO)
    cli.climate_command(
        ckey,
        mode=ClimateMode.COOL,
        target_temperature_low=20.0,
        target_temperature_high=24.0,
    )
    await asyncio.sleep(12)
    pairs = [
        (18.5, 26.5),
        (19.5, 25.5),
        (17.0, 28.0),
        (18.0, 27.0),
        (16.5, 29.5),
        (20.5, 23.5),
    ]
    if with_mode:
        pairs = pairs * 2
    bad = 0
    for n, (lo, hi) in enumerate(pairs):
        if with_mode:
            cli.climate_command(
                ckey, mode=ClimateMode.HEAT if n % 2 else ClimateMode.DRY
            )
            await asyncio.sleep(14)
        t0[0] = time.time()
        print(f"SET low={lo} high={hi}", flush=True)
        cli.climate_command(ckey, target_temperature_low=lo, target_temperature_high=hi)
        await asyncio.sleep(14)
        s = st.get("s")
        ok = s is not None and (
            s.target_temperature_low,
            s.target_temperature_high,
        ) == (lo, hi)
        bad += not ok
        got = (
            f"low={s.target_temperature_low} high={s.target_temperature_high}"
            if s
            else "no state"
        )
        print(
            f"RESULT low={lo} high={hi}: client {got} {'OK' if ok else 'MISMATCH'}",
            flush=True,
        )
    await cli.disconnect()
    await fake.disconnect()
    print(f"{bad} mismatches", flush=True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
