"""Press the "Dump allocations" button from allocring.yaml and save the dump.

usage: dump.py HOST OUT   env: API_KEY (unset for a plaintext API)
"""

import asyncio
from collections.abc import Callable
import contextlib
import pathlib
import re
import sys

from aioesphomeapi import APIClient, ButtonInfo
from aioesphomeapi.model import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text  # noqa: E402

HEADER = re.compile(r"\bDUMP n=(\d+) claimed=\d+ total=\d+ armed_ms=(\d+) busy=(\d+)")
ENTRY = re.compile(r"\bE (\d+) \d+ \d+ ")


async def dump(c: APIClient, write: Callable[[str], None], tries: int = 3) -> None:
    """Subscribe to logs, press the dump button and pass every line to WRITE until DUMP END.

    subscribe_logs does not confirm the subscription and the device drops log lines when busy, so the press is
    repeated (the dump is repeatable) until the header and all n entries arrived; exits 2 after TRIES presses.
    """
    ents, _ = await c.list_entities_services()
    button = next(
        e
        for e in ents
        if isinstance(e, ButtonInfo) and e.object_id == "dump_allocations"
    )
    seen: dict = {"n": None, "idx": set()}
    done = asyncio.Event()

    def on_log(msg) -> None:
        text = log_text(msg)
        write(text)
        if m := HEADER.search(text):
            seen["n"], seen["idx"] = int(m[1]), set()
            seen["armed"], seen["busy"] = int(m[2]) != 0, int(m[3])
        elif m := ENTRY.search(text):
            seen["idx"].add(int(m[1]))
        elif "DUMP END" in text and seen["n"] is not None:
            # without this press's header it is a late end from an earlier press
            done.set()

    c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_INFO)
    for _ in range(tries):
        await asyncio.sleep(2)
        seen["n"] = None
        done.clear()
        c.button_command(button.key)
        with contextlib.suppress(TimeoutError):
            await asyncio.wait_for(done.wait(), 120)
        # the device drops log lines when its send buffer is full, so require every entry
        if done.is_set() and seen["n"] is not None and not seen["armed"]:
            print(
                "the recorder was never armed (pressed before alloc_arm_s?)",
                file=sys.stderr,
            )
            sys.exit(2)
        if (
            done.is_set()
            and seen["n"] is not None
            and not seen["busy"]
            and seen["idx"] == set(range(seen["n"]))
        ):
            return
    print(f"no whole dump after {tries} presses", file=sys.stderr)
    sys.exit(2)


async def main() -> None:
    c = await connect(sys.argv[1])
    with pathlib.Path(sys.argv[2]).open("w", encoding="utf-8") as f:
        await dump(c, lambda s: f.write(s + "\n"))
    await c.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
