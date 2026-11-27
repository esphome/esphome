"""Subscribe with wake word audio, stop microWakeWord, say the wake word, start it, say it again, unsubscribe.

usage: mww.py HOST KEY   env: WAKE_AUDIO (default wake.aiff), PLAY (player command, default afplay)
Needs the buttons from mww-buttons.yaml. Expect no START while stopped and one after the start; exits 1 otherwise.
"""

import asyncio
import os
import sys
import time

from aioesphomeapi.model import LogLevel
from va_common import connect, log_text, mww_buttons, play

HOST, KEY = sys.argv[1], sys.argv[2]
WAKE = os.environ.get("WAKE_AUDIO", "wake.aiff")
T0 = time.monotonic()


def mark(w: str) -> None:
    print(f"{time.monotonic() - T0:8.3f}  {w}", flush=True)


async def main() -> int:
    cli = await connect(HOST, KEY)
    cli.subscribe_logs(
        lambda m: mark("LOG " + log_text(m)[:170]), log_level=LogLevel.LOG_LEVEL_DEBUG
    )
    ents, _ = await cli.list_entities_services()
    bstop, bstart = mww_buttons(ents)
    starts: list[float] = []

    async def start(*a: object) -> int:
        starts.append(time.monotonic())
        mark(f"START ww={a[4] if len(a) > 4 else None}")
        return 0

    async def stop(abort: bool) -> None:
        mark(f"STOP abort={abort}")

    async def audio(d1: bytes, d2: bytes) -> None:
        """Audio is not needed here."""

    unsub = cli.subscribe_voice_assistant(
        handle_start=start, handle_stop=stop, handle_audio=audio, wake_word_audio=True
    )
    await asyncio.sleep(4)
    mark("MWW stop")
    cli.button_command(bstop.key)
    await asyncio.sleep(4)
    mark("SAY while stopped")
    await play(WAKE)
    await asyncio.sleep(3)
    stopped_starts = len(starts)
    mark("MWW start")
    cli.button_command(bstart.key)
    await asyncio.sleep(4)
    mark("SAY running")
    await play(WAKE)
    await asyncio.sleep(5)
    mark("UNSUBSCRIBE")
    unsub()
    await asyncio.sleep(4)
    mark(f"starts while stopped={stopped_starts} total={len(starts)}")
    await cli.disconnect()
    return 0 if stopped_starts == 0 and len(starts) == 1 else 1


sys.exit(asyncio.run(main()))
