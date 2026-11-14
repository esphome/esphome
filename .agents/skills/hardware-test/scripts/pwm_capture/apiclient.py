"""Hold a plaintext API connection to the device; optionally send light transitions.

usage: apiclient.py HOST SECONDS [--every S] [--transition S]
With --every, alternates the first light between 30% and 70% brightness every S seconds with the given transition.
"""

import argparse
import asyncio
import pathlib
import sys
import time

from aioesphomeapi import LightInfo

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect  # noqa: E402


def log(msg: str) -> None:
    print(f"{time.strftime('%H:%M:%S')} {msg}", flush=True)


async def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("seconds", type=float)
    ap.add_argument("--every", type=float, default=0)
    ap.add_argument("--transition", type=float, default=1.0)
    a = ap.parse_args()
    cli = await connect(a.host)
    ents, _ = await cli.list_entities_services()
    key = next(e.key for e in ents if isinstance(e, LightInfo))
    states = 0

    def on_state(_state: object) -> None:
        nonlocal states
        states += 1

    cli.subscribe_states(on_state)
    log("connected")
    end = time.monotonic() + a.seconds
    cmds = 0
    while time.monotonic() < end:
        if not a.every:
            await asyncio.sleep(5)
            continue
        b = (0.3, 0.7)[cmds % 2]
        cli.light_command(key, state=True, brightness=b, transition_length=a.transition)
        log(f"cmd brightness={b} transition={a.transition}")
        cmds += 1
        await asyncio.sleep(a.every)
    log(f"done states={states} cmds={cmds}")
    await cli.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
