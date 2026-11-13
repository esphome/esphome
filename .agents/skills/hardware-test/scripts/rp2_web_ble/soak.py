"""API soak: subscribe states and logs for SECONDS, reconnect on drops, report the state count, uptime resets
(reboots), reset reason and crash or assert lines. usage: soak.py HOST SECONDS LOGFILE  env: API_KEY
Exits 1 on a drop, an uptime reset, a crash or assert line, or no states at all."""

import asyncio
import contextlib
import pathlib
import re
import sys
import time

from aioesphomeapi import LogLevel
from aioesphomeapi.model import SensorState, TextSensorState

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text, stop_event  # noqa: E402

BAD = re.compile(r"CRASH|panic|assert|HardFault|LWIP|lock not held", re.IGNORECASE)


async def main(host: str, secs: float, logf: str) -> None:
    end = time.monotonic() + secs
    states = drops = reboots = 0
    last_up = None
    reasons: set[str] = set()
    bad: list[str] = []
    with pathlib.Path(logf).open("a", encoding="utf-8") as out:

        def w(line: str) -> None:
            out.write(f"{time.strftime('%T')} {line}\n")

        while time.monotonic() < end:
            stopped, on_stop = stop_event()
            cli = await connect(host, tries=60, timeout=15, on_stop=on_stop)
            info = await cli.device_info()
            w(f"connected {info.name} {info.esphome_version} {info.compilation_time}")
            ents, _ = await cli.list_entities_services()
            names = {e.key: e.object_id for e in ents}

            def on_state(s, names: dict[int, str] = names) -> None:
                nonlocal states, last_up, reboots
                states += 1
                n = names.get(s.key, "")
                if isinstance(s, SensorState) and n == "uptime" and not s.missing_state:
                    if last_up is not None and s.state + 5 < last_up:
                        reboots += 1
                        w(f"UPTIME RESET {last_up} -> {s.state}")
                    last_up = s.state
                if isinstance(s, TextSensorState) and n == "reset_reason" and s.state:
                    reasons.add(s.state)

            def on_log(m) -> None:
                line = log_text(m)
                w(line)
                if BAD.search(line):
                    bad.append(line)

            cli.subscribe_states(on_state)
            cli.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
            with contextlib.suppress(TimeoutError):
                await asyncio.wait_for(stopped.wait(), max(0.0, end - time.monotonic()))
            if stopped.is_set():
                drops += 1
                w("DROPPED")
            with contextlib.suppress(Exception):
                await cli.disconnect()
    print(
        f"states={states} drops={drops} uptime_resets={reboots} last_uptime={last_up} "
        f"reset_reason={sorted(reasons)} bad_lines={len(bad)}"
    )
    for b in bad[:10]:
        print("BAD", b)
    if drops or reboots or bad or not states:
        sys.exit("FAIL")
    print("PASS")


asyncio.run(main(sys.argv[1], float(sys.argv[2]), sys.argv[3]))
