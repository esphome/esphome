"""Open and close the device's first cover N times with a states-only client, then save the allocation dump.

usage: cycles_cover.py HOST CYCLES OUT   env: API_KEY (unset for a plaintext API)
Exits 2 when a move did not reach its position (the cover never moved, so the dump proves nothing).
"""

import asyncio
import pathlib
import sys
import time

from aioesphomeapi import CoverInfo, CoverOperation

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect  # noqa: E402
from heap_alloc.dump import dump  # noqa: E402

# After each move, so allocations that come once the cover has stopped land in the same cycle
SETTLE_S = 8


async def main() -> None:
    host, cycles, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    with pathlib.Path(out).open("a", encoding="utf-8") as f:

        def w(s: str) -> None:
            f.write(time.strftime("%H:%M:%S ") + s + "\n")
            f.flush()

        c = await connect(host)
        ents, _ = await c.list_entities_services()
        cover = next(e for e in ents if isinstance(e, CoverInfo))
        states: asyncio.Queue = asyncio.Queue()
        c.subscribe_states(lambda s: s.key == cover.key and states.put_nowait(s))
        await asyncio.wait_for(states.get(), 10)

        async def move(pos: float) -> bool:
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
        for i in range(cycles):
            w(f"CYCLE {i + 1}")
            for pos in (1.0, 0.0):
                timeouts += not await move(pos)
                await asyncio.sleep(SETTLE_S)
        await dump(c, w)
        await c.disconnect()
    if timeouts:
        print(f"{timeouts} moves timed out", file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    asyncio.run(main())
