"""apilog.py HOST SECONDS OUT: subscribe VERBOSE logs and states for SECONDS with ms timestamps, reconnecting on a drop.
env: API_KEY (unset for a plaintext API). Prints the state count at the end."""

import asyncio
import contextlib
import pathlib
import sys
import time

from aioesphomeapi import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text, stop_event  # noqa: E402


def ts() -> str:
    return f"{time.time():.3f}"


async def main(host: str, secs: float, outf: str) -> None:
    end = time.monotonic() + secs
    states = 0
    with pathlib.Path(outf).open("a", encoding="utf-8") as out:

        def w(line: str) -> None:
            out.write(f"{ts()} {line}\n")
            out.flush()

        def on_state(_s) -> None:
            nonlocal states
            states += 1

        while time.monotonic() < end:
            stopped, on_stop = stop_event()
            cli = await connect(host, tries=60, timeout=15, on_stop=on_stop)
            w(f"API connected {(await cli.device_info()).name}")
            cli.subscribe_states(on_state)
            cli.subscribe_logs(
                lambda m: w(log_text(m)), log_level=LogLevel.LOG_LEVEL_VERBOSE
            )
            with contextlib.suppress(TimeoutError):
                await asyncio.wait_for(stopped.wait(), max(0.0, end - time.monotonic()))
            if stopped.is_set():
                w("API DROPPED")
            with contextlib.suppress(Exception):
                await cli.disconnect()
        w(f"API done states={states}")
    print(f"api states={states}")


asyncio.run(main(sys.argv[1], float(sys.argv[2]), sys.argv[3]))
