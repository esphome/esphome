"""Step stream-harness.yaml through frames, silence, garbage and radar restarts; print every state change.

usage: linkdrive.py HOST OUT   env: API_KEY (when the harness has one)
Lines are seconds since start: PRESS steps, STATE changes of the ld2450 entities (unknown when missing), and LOG lines
about the link or ld2450 warnings.
"""

import asyncio
import math
import os
import pathlib
import re
import sys
import time

from aioesphomeapi import APIClient, ButtonInfo, LogLevel

HOST, OUT = sys.argv[1], sys.argv[2]
LOG = pathlib.Path(OUT).open("w", encoding="utf-8")  # noqa: SIM115  open for the whole run
T0 = time.time()
STEPS = [
    ("Mode none", 8), ("Mode target", 6), ("Mode silent", 10), ("Mode none", 4), ("Mode target", 4),
    ("Mode garbage", 10), ("Mode target", 5),
    ("Radar Restart", 0.3), ("Mode silent", 3.5), ("Mode target", 6),
    ("Radar Restart", 0.3), ("Mode silent", 15), ("Mode none", 4),
]  # fmt: skip


def note(s: str) -> None:
    line = f"{time.time() - T0:7.2f} {s}"
    print(line)
    LOG.write(line + "\n")
    LOG.flush()


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=os.environ.get("API_KEY"))
    await c.connect(login=True)
    ents, _ = await c.list_entities_services()
    names = {e.key: e.name for e in ents}
    btn = {e.name: e.key for e in ents if isinstance(e, ButtonInfo)}
    last = {}

    def on_state(s) -> None:
        n = names.get(s.key)
        if n is None or n in btn:
            return
        v = (
            "unknown"
            if getattr(s, "missing_state", False)
            else getattr(s, "state", None)
        )
        if isinstance(v, float):
            v = "unknown (nan)" if math.isnan(v) else round(v, 1)
        if last.get(n) != v:
            last[n] = v
            note(f"STATE {n} = {v}")

    def on_log(m) -> None:
        t = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
        if re.search(r"[Ll]ink|\[W\]\[ld2450", t):
            note("LOG " + t.strip())

    c.subscribe_states(on_state)
    c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
    await asyncio.sleep(3)
    for name, wait in STEPS:
        note(f"PRESS {name}")
        c.button_command(btn[name])
        await asyncio.sleep(wait)
    await c.disconnect()
    # dev freezes entities during silence but still updates them on target frames; nothing at all is a dead stream
    if not last:
        note("NO-DATA: no entity state arrived")
        sys.exit(1)


asyncio.run(main())
