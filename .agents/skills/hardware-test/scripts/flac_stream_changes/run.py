"""run.py HOST BASEURL LABEL [all|formats|switch|leak]: drive the speaker media player through the test plan.

One API connection (API_KEY from the env, see apiconn.py) prints timestamped events; capture the device log
separately over serial. BASEURL serves the files named below (any HTTP server, e.g. `python3 -m http.server`).
Formats: every file played to the end. Switch: 12 URL changes mid play, 6 stop and play rounds, 5 announcements
over music, then one full play. Leak: 20 full plays.
"""

import asyncio
import pathlib
import sys
import time

from aioesphomeapi import MediaPlayerCommand, MediaPlayerEntityState, MediaPlayerInfo

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect  # noqa: E402

HOST, BASE, LABEL = sys.argv[1:4]
PHASE = sys.argv[4] if len(sys.argv) > 4 else "all"
FORMATS = [
    "s44b4096.flac",
    "s48b4608.flac",
    "m48b4096.flac",
    "m22b1152.flac",
    "s48b16384.flac",
    "s48_24.flac",
    "s44.mp3",
    "s44.wav",
]
SWITCH = [
    "s44b4096.flac",
    "m22b1152.flac",
    "s48b16384.flac",
    "m48b4096.flac",
    "s48b4608.flac",
    "s44b4096.flac",
    "m48b4096.flac",
    "s48b16384.flac",
    "m22b1152.flac",
    "s48b4608.flac",
    "s44b4096.flac",
    "s48b16384.flac",
]
LEAK = FORMATS[:5]
T0 = time.monotonic()
state: dict[str, str | None] = {"s": None}
failed: list[str] = []


def log(msg: str) -> None:
    print(f"{time.monotonic() - T0:8.2f} {LABEL} {msg}", flush=True)


async def wait_state(name: str, timeout: float) -> bool:
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if state["s"] == name:
            return True
        await asyncio.sleep(0.05)
    return False


async def main() -> None:
    c = await connect(HOST)
    ents, _ = await c.list_entities_services()
    p = next(e for e in ents if isinstance(e, MediaPlayerInfo))

    def on_state(s: object) -> None:
        if isinstance(s, MediaPlayerEntityState) and s.key == p.key:
            st = s.state.name
            if st != state["s"]:
                state["s"] = st
                log(f"STATE {st}")

    c.subscribe_states(on_state)
    await asyncio.sleep(1)

    def play(f: str, ann: bool = False) -> None:
        c.media_player_command(p.key, media_url=f"{BASE}/{f}", announcement=ann)

    async def stop() -> None:
        log("STOP")
        c.media_player_command(p.key, command=MediaPlayerCommand.STOP)
        await wait_state("IDLE", 5)

    async def play_full(f: str, timeout: float = 30) -> None:
        log(f"PLAY {f}")
        play(f)
        ok = await wait_state("PLAYING", 8)
        t = time.monotonic()
        done = await wait_state("IDLE", timeout)
        log(f"END {f} started={ok} idle={done} dur={time.monotonic() - t:.2f}")
        # 24-bit is rejected on dev (16-bit only)
        if not (ok and done) and "_24" not in f:
            failed.append(f)

    c.media_player_command(p.key, volume=0.5)
    if PHASE in ("all", "formats"):
        log("PHASE formats")
        for f in FORMATS:
            await play_full(f)
            await asyncio.sleep(3)  # plat.py needs two idle probes
    if PHASE in ("all", "switch"):
        log("PHASE switch")
        for f in SWITCH:  # change the stream mid play, no stop
            log(f"SWITCH {f}")
            play(f)
            await asyncio.sleep(2.5)
        for f in SWITCH[:6]:
            await stop()
            log(f"SWITCH {f}")
            play(f)
            await asyncio.sleep(2)
        await stop()
        log("PHASE announce")
        for _ in range(5):
            log("MUSIC long.flac")
            play("long.flac")
            await asyncio.sleep(3)
            log("ANN ann.flac")
            play("ann.flac", ann=True)
            await asyncio.sleep(5)
            log(f"STATE_NOW {state['s']}")
            await stop()
        await asyncio.sleep(3)
        await play_full("s44b4096.flac")
    if PHASE in ("all", "leak"):
        log("PHASE leak")
        for i in range(20):
            await play_full(LEAK[i % len(LEAK)])
            await asyncio.sleep(3)
        await asyncio.sleep(5)
    log("DONE")
    await c.disconnect()


asyncio.run(main())
if failed:
    sys.exit(f"{len(failed)} plays did not start or did not end: {' '.join(failed)}")
