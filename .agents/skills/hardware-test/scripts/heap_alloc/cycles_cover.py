"""Move the device's first cover to the far end and back N times with a states-only client, then save the dump.

usage: cycles_cover.py HOST CYCLES OUT   env: API_KEY (unset for a plaintext API)
Waits for the recorder's armed binary sensor first. Exits 2 when a move did not reach its position (the
cover never moved, so the dump proves nothing).
"""

import asyncio
import pathlib
import sys
import time

from aioesphomeapi import (
    BinarySensorInfo,
    BinarySensorState,
    CoverInfo,
    CoverOperation,
    CoverState,
)

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect  # noqa: E402
from heap_alloc.dump import dump  # noqa: E402

# After each move, so allocations that come once the cover has stopped land in the same cycle
SETTLE_S = 8
ARM_WAIT_S = 600


async def main() -> None:
    host, cycles, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    with pathlib.Path(out).open("w", encoding="utf-8") as f:

        def w(s: str) -> None:
            f.write(time.strftime("%H:%M:%S ") + s + "\n")
            f.flush()

        c = await connect(host)
        ents, _ = await c.list_entities_services()
        cover = next(e for e in ents if isinstance(e, CoverInfo))
        armed_info = next(
            e
            for e in ents
            if isinstance(e, BinarySensorInfo)
            and e.object_id == "allocation_recorder_armed"
        )
        states: asyncio.Queue = asyncio.Queue()
        armed = asyncio.Event()

        def on_state(s) -> None:
            if isinstance(s, CoverState) and s.key == cover.key:
                states.put_nowait(s)
            elif (
                isinstance(s, BinarySensorState) and s.key == armed_info.key and s.state
            ):
                armed.set()

        c.subscribe_states(on_state)
        latest = await asyncio.wait_for(states.get(), 10)
        # moves made before recording starts would be missing from the dump
        await asyncio.wait_for(armed.wait(), ARM_WAIT_S)
        w("ARMED")

        def drain() -> CoverState:
            """The newest queued state, so no stale one is read as this command's result."""
            nonlocal latest
            while not states.empty():
                latest = states.get_nowait()
            return latest

        async def move(pos: float) -> bool:
            drain()
            w(f"CMD {pos}")
            c.cover_command(cover.key, position=pos)
            moved = False
            deadline = time.monotonic() + 60
            while (left := deadline - time.monotonic()) > 0:
                try:
                    s = await asyncio.wait_for(states.get(), left)
                except TimeoutError:
                    break
                idle = s.current_operation == CoverOperation.IDLE
                moved |= not idle
                if moved and idle and abs(s.position - pos) < 0.01:
                    w(f"IDLE pos={s.position:.3f}")
                    return True
            w("TIMEOUT")
            return False

        timeouts = 0
        # start toward the far end, so the first command always moves the cover
        order = (0.0, 1.0) if drain().position > 0.5 else (1.0, 0.0)
        for i in range(cycles):
            w(f"CYCLE {i + 1}")
            for pos in order:
                timeouts += not await move(pos)
                await asyncio.sleep(SETTLE_S)
        if timeouts:
            # before the dump, which exits on its own failure
            print(f"{timeouts} moves timed out", file=sys.stderr)
        await dump(c, w)
        await c.disconnect()
    if timeouts:
        sys.exit(2)


if __name__ == "__main__":
    asyncio.run(main())
