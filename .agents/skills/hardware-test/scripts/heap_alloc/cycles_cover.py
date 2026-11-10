"""Open and close the device's first cover N times, then dump the allocation ring.

usage: cycles_cover.py HOST CYCLES OUT   env: API_KEY (unset for a plaintext API)
During the cycles the client subscribes to states only, so the API log path adds no allocations; it subscribes
to logs afterwards, presses the "Dump allocations" button from allocring.yaml and appends everything to OUT.
"""

import asyncio
import contextlib
import pathlib
import sys
import time

from aioesphomeapi import ButtonInfo, CoverInfo
from aioesphomeapi.model import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text  # noqa: E402


async def main() -> None:
    host, cycles, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    with pathlib.Path(out).open("a", encoding="utf-8") as f:
        await run(host, cycles, f)


async def run(host: str, cycles: int, f) -> None:

    def w(s: str) -> None:
        f.write(time.strftime("%H:%M:%S ") + s + "\n")
        f.flush()

    c = await connect(host)
    ents, _ = await c.list_entities_services()
    cover = next(e for e in ents if isinstance(e, CoverInfo))
    dump = next(e for e in ents if isinstance(e, ButtonInfo) and "dump" in e.object_id)
    cur = {}
    changed = asyncio.Event()

    def on_state(s) -> None:
        if s.key == cover.key:
            cur["s"] = s
            changed.set()

    c.subscribe_states(on_state)
    await asyncio.sleep(5)

    async def move(pos: float) -> None:
        w(f"CMD {pos}")
        c.cover_command(cover.key, position=pos)
        start, moved = time.time(), False
        while time.time() - start < 60:
            changed.clear()
            with contextlib.suppress(TimeoutError):
                await asyncio.wait_for(changed.wait(), 5)
            s = cur["s"]
            moved |= s.current_operation.name != "IDLE"
            if (
                moved
                and s.current_operation.name == "IDLE"
                and abs(s.position - pos) < 0.01
            ):
                w(f"IDLE pos={s.position:.3f}")
                return
        w("TIMEOUT")

    for i in range(cycles):
        w(f"CYCLE {i + 1}")
        await move(1.0)
        await asyncio.sleep(8)
        await move(0.0)
        await asyncio.sleep(8)

    done = asyncio.Event()

    def on_log(msg) -> None:
        text = log_text(msg)
        w(text)
        if "DUMP END" in text:
            done.set()

    c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_INFO)
    await asyncio.sleep(2)
    c.button_command(dump.key)
    await asyncio.wait_for(done.wait(), 120)
    await c.disconnect()


if __name__ == "__main__":
    asyncio.run(main())
