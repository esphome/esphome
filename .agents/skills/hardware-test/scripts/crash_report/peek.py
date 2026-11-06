"""Print the crash report capability and how many replayed 'CRASH DETECTED' lines a new log client gets.

usage: peek.py HOST   env: API_KEY (unset for a plaintext API). Press a crash button with api_button.py.
"""

import asyncio
import pathlib
import sys

from aioesphomeapi.model import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text  # noqa: E402


async def main() -> None:
    c = await connect(sys.argv[1], tries=60)
    caps = await c.device_capabilities()
    lines: list[str] = []
    c.subscribe_logs(
        lambda m: lines.append(log_text(m)),
        log_level=LogLevel.LOG_LEVEL_DEBUG,
        dump_config=True,
    )
    await asyncio.sleep(4)
    print(
        "caps",
        caps.crash_report,
        "replay lines",
        sum("CRASH DETECTED" in line for line in lines),
    )
    await c.disconnect()


asyncio.run(main())
