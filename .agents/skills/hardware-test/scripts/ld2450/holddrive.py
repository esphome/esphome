"""Time the presence hold on stream-harness.yaml and check presence_timeout survives a restart.

usage: holddrive.py HOST OUT   env: API_KEY (when the harness has one)
For timeouts 5 and 0: target frames for 3 s, then empty frames; prints the ms from the "mode none" press to the
has_target off line (up to one 100 ms frame of that is the harness). Then sets 0 and 20, restarts, reads the number.
A missing press line, off line or number state prints NO-DATA and the run exits 1.
"""

import asyncio
import os
import pathlib
import re
import sys

from aioesphomeapi import APIClient, ButtonInfo, LogLevel, NumberInfo, NumberState

HOST, OUT = sys.argv[1], sys.argv[2]
LOG = pathlib.Path(OUT).open("w", encoding="utf-8")  # noqa: SIM115  open for the whole run
MISSING = []


def note(s: str) -> None:
    print(s)
    LOG.write(s + "\n")
    LOG.flush()


async def session():
    c = APIClient(HOST, 6053, None, noise_psk=os.environ.get("API_KEY"))
    for attempt in range(60):
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
    num = next(
        e.key
        for e in ents
        if isinstance(e, NumberInfo) and e.name == "Presence Timeout"
    )
    st, lines = {}, []

    def on_state(s) -> None:
        if isinstance(s, NumberState) and s.key == num:
            st["t"] = s.state

    def on_log(m) -> None:
        t = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
        if "harness" in t:
            lines.append(t)
            LOG.write("LOG " + t + "\n")

    c.subscribe_states(on_state)
    c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
    for _ in range(20):
        await asyncio.sleep(1)
        if "t" in st:
            break
    # a freshly booted ESP8266 delays API commands that arrive too early
    await asyncio.sleep(8)
    return c, btn, num, st, lines


def first_ms(lines: list[str], pat: str) -> int | None:
    for line in lines:
        if m := re.search(pat + r" at (\d+)", line):
            return int(m.group(1))
    return None


def number(st: dict, what: str) -> str:
    if "t" not in st:
        MISSING.append(what)
        return "NO-DATA"
    return f"{st['t']:g}"


async def hold(c, btn, num, lines, timeout: int) -> None:
    c.number_command(num, timeout)
    await asyncio.sleep(2)
    c.button_command(btn["Mode target"])
    await asyncio.sleep(3)
    lines.clear()  # only this run's press and off lines count
    c.button_command(btn["Mode none"])
    await asyncio.sleep(timeout + 8)
    start, off = first_ms(lines, "mode none"), first_ms(lines, "presence raw OFF")
    if start is None or off is None or off < start:
        MISSING.append(f"hold {timeout}")
        note(f"HOLD timeout={timeout}: NO-DATA (press line {start}, off line {off})")
    else:
        note(
            f"HOLD timeout={timeout}: off {off - start} ms after the last target frame"
        )


async def main() -> None:
    c, btn, num, st, lines = await session()
    note(f"START number {number(st, 'start')}")
    for t in (5, 0):
        await hold(c, btn, num, lines, t)
    for t in (0, 20):
        st.pop("t", None)
        c.number_command(num, t)
        await asyncio.sleep(2)
        note(f"SET {t}, number reads {number(st, f'set {t}')}")
        c.button_command(btn["Restart"])
        await asyncio.sleep(1)
        await c.disconnect()
        await asyncio.sleep(15)
        c, btn, num, st, lines = await session()
        note(f"AFTER RESTART number reads {number(st, f'restart {t}')}")
    await c.disconnect()
    if MISSING:
        note(f"NO-DATA for: {', '.join(MISSING)}")
        sys.exit(1)


asyncio.run(main())
