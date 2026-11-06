"""Stale wake word audio check: say a marker word, stop microWakeWord for 3 s, start it, say the wake word.

usage: stale.py HOST KEY OUT TRIALS
env: MARKER_AUDIO (default marker.aiff), WAKE_AUDIO (default wake.aiff), PLAY (player command, default afplay)

The wake word clip of each detection must not reach back to the marker word spoken before the restart. Writes a log
to OUT, the received audio to OUT.t<N>.pcm and a RESULT line per trial; stalean.py measures the clips. Each run is
rejected 0.6 s after the start. Needs the buttons from mww-buttons.yaml. Exits 1 when nothing was detected.
"""

import asyncio
import json
import os
import pathlib
import re
import socket
import subprocess
import sys
import time

from aioesphomeapi import APIClient, VoiceAssistantEventType as E
from aioesphomeapi.model import LogLevel

HOST, KEY, OUT, N = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
MARKER = os.environ.get("MARKER_AUDIO", "marker.aiff")
WAKE = os.environ.get("WAKE_AUDIO", "wake.aiff")
PLAY = os.environ.get("PLAY", "afplay").split()
T0 = time.monotonic()
LOG = pathlib.Path(OUT).open("w", encoding="utf-8")  # noqa: SIM115  open for the whole run
LOG_RE = re.compile(r"voice_assistant|micro_wake_word|hwt")


def mark(w: str) -> None:
    line = f"{time.monotonic() - T0:8.3f}  {w}"
    print(line, flush=True)
    LOG.write(line + "\n")


async def play(f: str) -> None:
    await asyncio.to_thread(subprocess.run, [*PLAY, f], check=True)


async def main() -> int:
    cli = APIClient(
        socket.gethostbyname(HOST), 6053, None, noise_psk=None if KEY == "-" else KEY
    )
    await cli.connect(login=True)

    def on_log(m) -> None:
        line = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
        if LOG_RE.search(line):
            mark("LOG " + line[:150])

    cli.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
    ents, _ = await cli.list_entities_services()
    bstop = next(e for e in ents if e.object_id == "hwtest_mww_stop")
    bstart = next(e for e in ents if e.object_id == "hwtest_mww_start")
    cur: dict = {}
    loop = asyncio.get_running_loop()

    async def reject() -> None:
        await asyncio.sleep(0.6)
        cli.send_voice_assistant_event(E.VOICE_ASSISTANT_WAKE_WORD_REJECTED, None)

    async def start(
        conv: str, flags: int, settings: object, phrase: str | None, ww: object = None
    ) -> int:
        cur["ww"] = ww
        cur["a"] = bytearray()
        mark(f"START ww bytes={ww.audio_bytes if ww else None}")
        cur["ev"].set()
        cur["task"] = loop.create_task(reject())
        return 0

    async def stop(abort: bool) -> None:
        """The run ends on our rejection."""

    async def audio(d1: bytes, d2: bytes) -> None:
        if "a" in cur:
            cur["a"] += d1

    cli.subscribe_voice_assistant(
        handle_start=start, handle_stop=stop, handle_audio=audio, wake_word_audio=True
    )
    await asyncio.sleep(5)
    detected = 0
    for t in range(N):
        cur.clear()
        cur["ev"] = asyncio.Event()
        mark(f"TRIAL {t}")
        await play(MARKER)
        cli.button_command(bstop.key)
        await asyncio.sleep(3)
        cli.button_command(bstart.key)
        await asyncio.sleep(0.3)
        await play(WAKE)
        try:
            await asyncio.wait_for(cur["ev"].wait(), 4)
        except TimeoutError:
            mark(f"RESULT {json.dumps({'t': t, 'detected': False})}")
            await asyncio.sleep(3)
            continue
        await asyncio.sleep(1.5)
        detected += 1
        pathlib.Path(f"{OUT}.t{t}.pcm").write_bytes(bytes(cur["a"]))
        ww_ms = cur["ww"].audio_bytes // 32 if cur["ww"] else 0
        mark(f"RESULT {json.dumps({'t': t, 'detected': True, 'ww_ms': ww_ms})}")
        await asyncio.sleep(7)
    await cli.disconnect()
    return 0 if detected else 1


RC = asyncio.run(main())
LOG.close()
sys.exit(RC)
