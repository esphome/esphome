"""Press the client's buttons in order and log every client line about BLE security.

usage: drive.py CLIENT_HOST OUT STEP...   STEP: a button name, or wait:SECONDS   env: API_KEY
"""

import asyncio
import os
import pathlib
import re
import sys
import time

from aioesphomeapi import APIClient, ButtonInfo, LogLevel

HOST, OUT, STEPS = sys.argv[1], sys.argv[2], sys.argv[3:]
LOG = pathlib.Path(OUT).open("w", encoding="utf-8")  # noqa: SIM115  open for the whole run
PATTERN = re.compile(
    r"CLI|ble_client|ble_text_sensor|esp32_ble_client|[Aa]uth|[Ee]ncrypt|[Bb]ond|Error|status=|pair"
)


def note(s: str) -> None:
    line = f"{time.strftime('%H:%M:%S')} {s}"
    print(line)
    LOG.write(line + "\n")
    LOG.flush()


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=os.environ.get("API_KEY"))
    for attempt in range(40):
        try:
            await c.connect(login=True)
            break
        except Exception as err:  # noqa: BLE001
            last_err = err
            note(f"connect attempt {attempt + 1} failed: {err!r}")
            await asyncio.sleep(2)
    else:
        raise last_err
    ents, _ = await c.list_entities_services()
    btn = {e.name: e.key for e in ents if isinstance(e, ButtonInfo)}

    def on_log(m) -> None:
        t = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
        if PATTERN.search(t):
            note("LOG " + t.strip().replace("\n", " | "))

    c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
    for step in STEPS:
        if step.startswith("wait:"):
            await asyncio.sleep(float(step[5:]))
            continue
        note(f"PRESS {step}")
        c.button_command(btn[step])
        await asyncio.sleep(4)
    await c.disconnect()


asyncio.run(main())
