"""svcrun.py HOST ACTION DONE_REGEX TIMEOUT: run a user defined API action, collect the logs until DONE_REGEX matches
or TIMEOUT seconds pass, and print the result lines. env: API_KEY (unset for a plaintext API).
Exits 1 on a timeout or when a result line says FAIL, FAILED or crash."""

import asyncio
import pathlib
import re
import sys
import time

from aioesphomeapi import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text  # noqa: E402

RESULT = re.compile(
    r"RESULT|TEST PASSED|TEST FAILED|Test Complete|crash", re.IGNORECASE
)


async def main(host: str, action: str, done: str, timeout: float) -> int:
    cli = await connect(host, tries=30, timeout=15)
    lines: list[str] = []
    finished = asyncio.Event()
    rx = re.compile(done)

    def on_log(msg) -> None:
        line = log_text(msg)
        lines.append(line)
        if rx.search(line):
            finished.set()

    cli.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
    _, services = await cli.list_entities_services()
    svc = next((s for s in services if s.name == action), None)
    if svc is None:
        sys.exit(f"no action {action} on {host}")
    await asyncio.sleep(0.5)
    t0 = time.monotonic()
    await cli.execute_service(svc, {})
    try:
        await asyncio.wait_for(finished.wait(), timeout)
        status = "DONE"
    except TimeoutError:
        status = "TIMEOUT"
    found = [line for line in lines if RESULT.search(line)]
    for line in found:
        print("  " + line)
    print(f"{action}: {status} after {time.monotonic() - t0:.1f} s")
    await cli.disconnect()
    failed = any(
        re.search(r"\bFAIL(ED)?\b|crash", line, re.IGNORECASE) for line in found
    )
    return 1 if status != "DONE" or failed else 0


sys.exit(asyncio.run(main(sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4]))))
