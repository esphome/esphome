"""Time the presence hold on stream-harness.yaml and check presence_timeout survives a restart.

usage: holddrive.py HOST OUT   env: API_KEY (when the harness has one)
For timeouts 5 and 0: target frames for 3 s, then empty frames; prints the ms from the "mode none" press to the
has_target off line (up to one 100 ms frame of that is the harness). Then sets 0 and 20, restarts, reads the number.
"""

import asyncio
import os
import pathlib
import re
import sys

from aioesphomeapi import APIClient, ButtonInfo, LogLevel, NumberInfo, NumberState

HOST, OUT = sys.argv[1], sys.argv[2]
LOG = pathlib.Path(OUT).open("w", encoding="utf-8")  # noqa: SIM115  open for the whole run


def note(s: str) -> None:
    print(s)
    LOG.write(s + "\n")
    LOG.flush()


async def session():
    c = APIClient(HOST, 6053, None, noise_psk=os.environ.get("API_KEY"))
    for _ in range(60):
        try:
            await c.connect(login=True)
            break
        except Exception:  # noqa: BLE001
            await asyncio.sleep(2)
    ents, _ = await c.list_entities_services()
    btn = {e.name: e.key for e in ents if isinstance(e, ButtonInfo)}
    num = next(
        e.key
        for e in ents
        if isinstance(e, NumberInfo) and e.name == "Presence Timeout"
    )
    st, lines = {}, []
    c.subscribe_states(
        lambda s: (
            st.__setitem__("t", s.state)
            if isinstance(s, NumberState) and s.key == num
            else None
        )
    )

    def on_log(m) -> None:
        t = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
        if "harness" in t:
            lines.append(t)
            LOG.write("LOG " + t + "\n")

    c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
    for _ in range(20):
        await asyncio.sleep(1)
        if "t" in st:
            break
    await asyncio.sleep(
        8
    )  # a freshly booted ESP8266 delays API commands that arrive too early
    return c, btn, num, st, lines


def last_ms(lines: list[str], pat: str) -> int | None:
    for line in reversed(lines):
        if m := re.search(pat + r" at (\d+)", line):
            return int(m.group(1))
    return None


async def hold(c, btn, num, lines, timeout: int) -> None:
    c.number_command(num, timeout)
    await asyncio.sleep(2)
    c.button_command(btn["Mode target"])
    await asyncio.sleep(3)
    c.button_command(btn["Mode none"])
    await asyncio.sleep(timeout + 8)
    start, off = last_ms(lines, "mode none"), last_ms(lines, "presence raw OFF")
    note(
        f"HOLD timeout={timeout}: off {off - start if start and off and off > start else None} ms after the last target frame"
    )


async def main() -> None:
    c, btn, num, st, lines = await session()
    note(f"START number {st.get('t')}")
    for t in (5, 0):
        await hold(c, btn, num, lines, t)
    for t in (0, 20):
        c.number_command(num, t)
        await asyncio.sleep(2)
        note(f"SET {t}, number reads {st.get('t')}")
        c.button_command(btn["Restart"])
        await asyncio.sleep(1)
        await c.disconnect()
        await asyncio.sleep(15)
        c, btn, num, st, lines = await session()
        note(f"AFTER RESTART number reads {st.get('t')}")
    await c.disconnect()


asyncio.run(main())
