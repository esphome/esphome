"""Drive the modem test build over the API: press buttons, print sensor states, save the device log.
usage: mdrive.py HOST LOGFILE OP [OP ...]
  OP: list | press:NAME | wait:SECONDS
"""

import asyncio
import pathlib
import sys
import time

from aioesphomeapi import (
    APIClient,
    ButtonInfo,
    LogLevel,
    SensorInfo,
    SensorState,
    TextSensorInfo,
    TextSensorState,
)

HOST, LOGFILE = sys.argv[1], sys.argv[2]
OPS = sys.argv[3:]
T0 = time.time()


def stamp() -> str:
    return f"{time.time() - T0:7.3f}"


async def main() -> None:
    with pathlib.Path(LOGFILE).open("w", encoding="utf-8") as log:
        c = APIClient(HOST, 6053, None)
        await c.connect(login=True)
        info = await c.device_info()
        print(
            f"device {info.name} esphome {info.esphome_version} compiled {info.compilation_time}"
        )
        ents, _ = await c.list_entities_services()
        by_key = {e.key: e for e in ents}
        buttons = {e.name: e for e in ents if isinstance(e, ButtonInfo)}

        def on_state(s) -> None:
            e = by_key.get(s.key)
            if e is None:
                return
            if isinstance(s, TextSensorState):
                print(
                    f"{stamp()} TXT {e.name} = {s.state!r}{' (missing)' if s.missing_state else ''}",
                    flush=True,
                )
            elif isinstance(s, SensorState) and isinstance(e, SensorInfo):
                print(f"{stamp()} SEN {e.name} = {s.state:.1f}", flush=True)

        def on_log(msg) -> None:
            log.write(f"{stamp()} {msg.message.decode(errors='replace')}\n")
            log.flush()

        c.subscribe_states(on_state)
        c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
        await asyncio.sleep(1.5)
        for op in OPS:
            if op == "list":
                for n in sorted(buttons):
                    print("button:", n)
                for e in ents:
                    if isinstance(e, (TextSensorInfo, SensorInfo)):
                        print("entity:", type(e).__name__, e.name)
                continue
            kind, _, rest = op.partition(":")
            if kind == "wait":
                await asyncio.sleep(float(rest))
                continue
            if kind == "press":
                if rest not in buttons:
                    sys.exit(f"no button named {rest!r}; have {sorted(buttons)}")
                print(f"{stamp()} >>> press {rest}", flush=True)
                log.write(f"{stamp()} >>> press {rest}\n")
                c.button_command(buttons[rest].key)
            else:
                sys.exit(f"unknown op {op!r}")
            await asyncio.sleep(2.0)
        await asyncio.sleep(1)
        await c.disconnect()


asyncio.run(main())
