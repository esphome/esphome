"""API soak: subscribe states and logs for SECONDS, reconnect on drops, report state count, uptime resets
(reboots), reset reason and crash/assert lines. usage: soak.py HOST SECONDS LOGFILE  env API_KEY"""

import asyncio
import contextlib
import os
import pathlib
import re
import sys
import time

from aioesphomeapi import APIClient, LogLevel
from aioesphomeapi.model import SensorState, TextSensorState

BAD = re.compile(r"CRASH|panic|assert|HardFault|LWIP|lock not held", re.IGNORECASE)


async def main(host: str, secs: float, logf: str) -> None:
    end = time.monotonic() + secs
    states = drops = reboots = 0
    last_up = None
    reasons: set[str] = set()
    bad: list[str] = []
    out = pathlib.Path(logf).open("a", encoding="utf-8")  # noqa: SIM115
    names: dict[int, str] = {}
    while time.monotonic() < end:
        cli = APIClient(host, 6053, None, noise_psk=os.environ["API_KEY"])
        try:
            await asyncio.wait_for(cli.connect(login=True), 15)
        except Exception as e:  # noqa: BLE001
            out.write(f"{time.strftime('%T')} connect failed {type(e).__name__}\n")
            out.flush()
            await asyncio.sleep(2)
            continue
        info = await cli.device_info()
        out.write(
            f"{time.strftime('%T')} connected {info.name} {info.esphome_version} {info.compilation_time}\n"
        )
        ents, _ = await cli.list_entities_services()
        names = {e.key: e.object_id for e in ents}

        def on_state(s, names: dict[int, str] = names) -> None:
            nonlocal states, last_up, reboots
            states += 1
            n = names.get(s.key, "")
            if isinstance(s, SensorState) and n == "uptime" and not s.missing_state:
                if last_up is not None and s.state + 5 < last_up:
                    reboots += 1
                    out.write(
                        f"{time.strftime('%T')} UPTIME RESET {last_up} -> {s.state}\n"
                    )
                last_up = s.state
            if isinstance(s, TextSensorState) and n == "reset_reason" and s.state:
                reasons.add(s.state)

        def on_log(m) -> None:
            line = m.message.decode(errors="replace")
            out.write(f"{time.strftime('%T')} {line}\n")
            if BAD.search(line):
                bad.append(line)

        cli.subscribe_states(on_state)
        cli.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
        cli.set_on_disconnect = None
        try:
            while time.monotonic() < end:
                await asyncio.sleep(1)
                if not cli._connection or not cli._connection.is_connected:  # noqa: SLF001
                    raise ConnectionError("dropped")
        except Exception:  # noqa: BLE001
            drops += 1
            out.write(f"{time.strftime('%T')} DROPPED\n")
        with contextlib.suppress(Exception):
            await cli.disconnect()
    out.close()
    print(
        f"states={states} drops={drops} uptime_resets={reboots} last_uptime={last_up} "
        f"reset_reason={sorted(reasons)} bad_lines={len(bad)}"
    )
    for b in bad[:10]:
        print("BAD", b)


asyncio.run(main(sys.argv[1], float(sys.argv[2]), sys.argv[3]))
