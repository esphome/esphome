"""apilog.py HOST SECONDS OUT [check]: subscribe VERBOSE logs and states, ms timestamps; reconnect on drop.
env API_KEY. With 'check', connect once, print device name and state count over SECONDS, exit."""

import asyncio
import os
import pathlib
import sys
import time

from aioesphomeapi import APIClient, LogLevel


def ts() -> str:
    return f"{time.time():.3f}"


async def main(host: str, secs: float, outf: str) -> None:
    end = time.monotonic() + secs
    out = pathlib.Path(outf).open("a", encoding="utf-8")  # noqa: SIM115
    states = 0
    while time.monotonic() < end:
        cli = APIClient(host, 6053, None, noise_psk=os.environ["API_KEY"])
        try:
            await asyncio.wait_for(cli.connect(login=True), 15)
        except Exception as e:  # noqa: BLE001
            out.write(f"{ts()} API connect failed {type(e).__name__} {e}\n")
            out.flush()
            await asyncio.sleep(2)
            continue
        info = await cli.device_info()
        out.write(f"{ts()} API connected {info.name}\n")
        out.flush()

        def on_state(s) -> None:
            nonlocal states
            states += 1

        def on_log(m) -> None:
            out.write(f"{ts()} {m.message.decode(errors='replace')}\n")
            out.flush()

        cli.subscribe_states(on_state)
        cli.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_VERBOSE)
        try:
            while time.monotonic() < end:
                await asyncio.sleep(1)
                if not cli._connection or not cli._connection.is_connected:  # noqa: SLF001
                    raise ConnectionError("dropped")
        except Exception:  # noqa: BLE001
            out.write(f"{ts()} API DROPPED\n")
            out.flush()
        finally:
            await cli.disconnect()
    out.write(f"{ts()} API done states={states}\n")
    print(f"api states={states}")


asyncio.run(main(sys.argv[1], float(sys.argv[2]), sys.argv[3]))
