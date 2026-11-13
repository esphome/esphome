"""Record every 'M ...' marker from the device log while driving the automation trace config.

usage: drive.py HOST OUT   env: API_KEY (unset for a plaintext API)

Order: wait for the boot run's 'M done', press Run All and wait for 'M done' again, press Chain Button twice, set Num
70 then 20, toggle Sw on and off, select c then a. Writes one marker per line to OUT (timestamps dropped). Exits 1
when the Run All pass never reached 'M done'.
"""

import asyncio
import pathlib
import re
import sys

from aioesphomeapi import LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect, log_text  # noqa: E402

HOST, OUT = sys.argv[1], sys.argv[2]
marks: list[str] = []
done = asyncio.Event()


async def main() -> bool:
    cli = await connect(HOST)

    def on_log(m) -> None:
        if mm := re.search(r"\]: (M .*)$", log_text(m)):
            marks.append(mm.group(1).strip())
            if mm.group(1).strip() == "M done":
                done.set()

    cli.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
    infos, _ = await cli.list_entities_services()
    by = {i.name: i for i in infos}
    try:
        await asyncio.wait_for(done.wait(), 40)
    except TimeoutError:
        marks.append("DRIVER boot run not seen")
    marks.append("DRIVER press Run All")
    done.clear()
    cli.button_command(by["Run All"].key)
    ok = True
    try:
        await asyncio.wait_for(done.wait(), 40)
    except TimeoutError:
        marks.append("DRIVER run all timed out")
        ok = False
    steps = (
        ("cb1", lambda: cli.button_command(by["Chain Button"].key)),
        ("cb2", lambda: cli.button_command(by["Chain Button"].key)),
        ("num70", lambda: cli.number_command(by["Num"].key, 70.0)),
        ("num20", lambda: cli.number_command(by["Num"].key, 20.0)),
        ("swon", lambda: cli.switch_command(by["Sw"].key, True)),
        ("swoff", lambda: cli.switch_command(by["Sw"].key, False)),
        ("selc", lambda: cli.select_command(by["Sel"].key, "c")),
        ("sela", lambda: cli.select_command(by["Sel"].key, "a")),
    )
    for label, send in steps:
        marks.append(f"DRIVER {label}")
        send()
        await asyncio.sleep(0.8)
    await asyncio.sleep(1)
    await cli.disconnect()
    return ok


OK = asyncio.run(main())
pathlib.Path(OUT).write_text("\n".join(marks) + "\n", encoding="utf-8")
print(f"{len(marks)} lines")
sys.exit(0 if OK else 1)
