"""Press the "Dump allocations" button from allocring.yaml and save the dump.

usage: dump.py HOST OUT   env: API_KEY (unset for a plaintext API)
"""

import asyncio
from collections.abc import Callable
import pathlib
import sys

from aioesphomeapi import APIClient, ButtonInfo
from aioesphomeapi.model import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text  # noqa: E402


async def dump(c: APIClient, write: Callable[[str], None]) -> None:
    """Subscribe to logs, press the dump button and pass every line to WRITE until DUMP END."""
    ents, _ = await c.list_entities_services()
    button = next(
        e
        for e in ents
        if isinstance(e, ButtonInfo) and e.object_id == "dump_allocations"
    )
    done = asyncio.Event()

    def on_log(msg) -> None:
        text = log_text(msg)
        write(text)
        if "DUMP END" in text:
            done.set()

    c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_INFO)
    await asyncio.sleep(2)  # subscribe_logs does not confirm the subscription
    c.button_command(button.key)
    await asyncio.wait_for(done.wait(), 120)


async def main() -> None:
    c = await connect(sys.argv[1])
    with pathlib.Path(sys.argv[2]).open("a", encoding="utf-8") as f:
        await dump(c, lambda s: f.write(s + "\n"))
    await c.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
