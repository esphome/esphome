"""Check over the API that each board runs the expected build.
Usage: apiinfo.py HOST[=BUILD_INFO_JSON]...
Prints name and compilation time per host. With a build_info.json (from the build directory), the board's
compilation time must equal that build's build_time_str. Exits 1 when a host fails or runs another build."""

import asyncio
import json
from pathlib import Path
import sys

from aioesphomeapi import APIClient, APIConnectionError


async def info(host: str) -> tuple[str, str]:
    cli = APIClient(host, 6053, None)
    await cli.connect(login=True)
    try:
        i = await cli.device_info()
    finally:
        await cli.disconnect()
    return i.name, i.compilation_time


bad = 0
for arg in sys.argv[1:]:
    host, _, build_info = arg.partition("=")
    try:
        name, built = asyncio.run(asyncio.wait_for(info(host), 30))
    except (APIConnectionError, TimeoutError, OSError) as err:
        print("API", host, "ERROR", repr(err), flush=True)
        bad += 1
        continue
    want = (
        json.loads(Path(build_info).read_text(encoding="utf-8"))["build_time_str"]
        if build_info
        else None
    )
    ok = want is None or built == want
    bad += not ok
    print("API", host, name, built, "" if ok else f"MISMATCH want {want}", flush=True)
sys.exit(1 if bad else 0)
