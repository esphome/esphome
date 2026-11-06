"""Record every 'M ...' marker from the device log while driving the automation trace config.

usage: drive.py HOST OUT   env: API_KEY (unset for a plaintext API)

Order: wait for the boot run's 'M done', press Run All and wait for 'M done' again, press Chain Button twice, set Num
70 then 20, toggle Sw on and off, select c then a. Writes one marker per line to OUT (timestamps dropped). Exits 1
when the Run All pass never reached 'M done'.
"""

import asyncio
import os
import pathlib
import re
import sys

from aioesphomeapi import APIClient, LogLevel

HOST, OUT = sys.argv[1], sys.argv[2]
marks: list[str] = []
done = asyncio.Event()


async def main() -> bool:
    cli = APIClient(HOST, 6053, None, noise_psk=os.environ.get("API_KEY"))
    for _ in range(30):
        try:
            await cli.connect(login=True)
            break
        except Exception:  # noqa: BLE001
            await asyncio.sleep(1)
    else:
        sys.exit(f"no API connection to {HOST}")

    def on_log(m) -> None:
        t = re.sub(r"\x1b\[[0-9;]*m", "", m.message.decode(errors="replace"))
        if mm := re.search(r"\]: (M .*)$", t):
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
    for step in ("cb1", "cb2", "num70", "num20", "swon", "swoff", "selc", "sela"):
        marks.append(f"DRIVER {step}")
        if step.startswith("cb"):
            cli.button_command(by["Chain Button"].key)
        elif step.startswith("num"):
            cli.number_command(by["Num"].key, float(step[3:]))
        elif step.startswith("sw"):
            cli.switch_command(by["Sw"].key, step == "swon")
        else:
            cli.select_command(by["Sel"].key, step[3:])
        await asyncio.sleep(0.8)
    await asyncio.sleep(1)
    await cli.disconnect()
    return ok


OK = asyncio.run(main())
pathlib.Path(OUT).write_text("\n".join(marks) + "\n", encoding="utf-8")
print(f"{len(marks)} lines")
sys.exit(0 if OK else 1)
