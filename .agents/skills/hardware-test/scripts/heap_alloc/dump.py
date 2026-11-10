"""Press the "Dump allocations" button from allocring.yaml and save the dump.

usage: dump.py HOST OUT   env: API_KEY (unset for a plaintext API)
"""

import asyncio
from collections.abc import Callable
import contextlib
import pathlib
import sys

from aioesphomeapi import APIClient, ButtonInfo
from aioesphomeapi.model import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text  # noqa: E402


async def dump(c: APIClient, write: Callable[[str], None], tries: int = 3) -> None:
    """Subscribe to logs, press the dump button and pass every line to WRITE until DUMP END.

    subscribe_logs does not confirm the subscription, so the press is repeated (the dump is repeatable) until
    the DUMP header arrived before DUMP END; exits 2 after TRIES presses without a whole dump.
    """
    ents, _ = await c.list_entities_services()
    button = next(
        e
        for e in ents
        if isinstance(e, ButtonInfo) and e.object_id == "dump_allocations"
    )
    seen = {"header": False}
    done = asyncio.Event()

    def on_log(msg) -> None:
        text = log_text(msg)
        write(text)
        if "DUMP n=" in text:
            seen["header"] = True
        elif "DUMP END" in text:
            done.set()

    c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_INFO)
    for _ in range(tries):
        await asyncio.sleep(2)
        seen["header"] = False
        done.clear()
        c.button_command(button.key)
        with contextlib.suppress(TimeoutError):
            await asyncio.wait_for(done.wait(), 120)
        if done.is_set() and seen["header"]:
            return
    print(f"no whole dump after {tries} presses", file=sys.stderr)
    sys.exit(2)


async def main() -> None:
    c = await connect(sys.argv[1])
    with pathlib.Path(sys.argv[2]).open("a", encoding="utf-8") as f:
        await dump(c, lambda s: f.write(s + "\n"))
    await c.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
