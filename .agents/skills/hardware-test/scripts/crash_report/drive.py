"""Press a crash button, reconnect, fetch the crash report twice; print capabilities around it and the replayed log.

usage: drive.py HOST BUTTON_OBJECT_ID   env: API_KEY (unset for a plaintext API)
An earlier report is read first so it cannot pass for this crash. Exits 1 when the device still answers after the
press, does not offer a crash report after the crash, or the report has no reason or a PC of 0.
"""

import asyncio
import pathlib
import re
import sys
import time

from aioesphomeapi import APIClient, ButtonInfo
from aioesphomeapi.model import CrashReason, LogLevel

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import apiconn  # noqa: E402

HOST, BTN = sys.argv[1], sys.argv[2]
T0 = time.monotonic()


def mark(w: str) -> None:
    print(f"{time.monotonic() - T0:7.2f}  {w}", flush=True)


async def connect() -> APIClient:
    return await apiconn.connect(HOST, tries=90)


async def main() -> str | None:
    """Run the flow; returns why it failed, or None."""
    c = await connect()
    before = (await c.device_capabilities()).crash_report
    mark(f"before: crash_report={before}")
    if before:
        # reading it drops the record at the next reboot, so only a new crash leaves one
        old = await c.crash_report()
        mark(f"read the earlier report first: reason={old.reason}")
    ents, _ = await c.list_entities_services()
    key = next((e.key for e in ents if e.object_id == BTN), None)
    if key is None:
        await c.disconnect()
        buttons = [e.object_id for e in ents if isinstance(e, ButtonInfo)]
        return f"no button {BTN!r}, the device has {buttons}"
    mark(f"PRESS {BTN}")
    c.button_command(key)
    await asyncio.sleep(2)
    try:
        await asyncio.wait_for(c.device_info(), 3)
    except Exception:  # noqa: BLE001
        mark("no answer after the press (expected)")
    else:
        await c.disconnect()
        return (
            "the device still answers after the press, so the button did not crash it"
        )
    try:
        await c.disconnect()
    except Exception:  # noqa: BLE001
        mark("disconnect after the crash failed (expected)")
    await asyncio.sleep(3)
    c = await connect()
    mark("reconnected")
    offered = (await c.device_capabilities()).crash_report
    mark(f"after crash: crash_report={offered}")
    r = await c.crash_report()
    mark(
        f"REPORT1 flags={r.flags!r} reason={r.reason} pc={r.pc:#010x} cause={r.cause} "
        f"fault={r.fault_address:#010x} build_time={r.build_time} crashed_core={r.crashed_core}"
    )
    for core in r.cores:
        mark(f"  core {core.core}: " + " ".join(f"{a:#010x}" for a in core.backtrace))
    r2 = await c.crash_report()
    mark(f"REPORT2 flags={r2.flags!r} reason={r2.reason} cores={len(r2.cores)}")
    mark(f"after read: crash_report={(await c.device_capabilities()).crash_report}")
    lines: list[str] = []
    c.subscribe_logs(
        lambda m: lines.append(apiconn.log_text(m)),
        log_level=LogLevel.LOG_LEVEL_VERY_VERBOSE,
        dump_config=True,
    )
    await asyncio.sleep(4)
    for line in lines:
        if re.search(
            r"crash|Crash|CRASH|PC|BT\d|backtrace|Backtrace|reset reason|Reset", line
        ):
            mark("LOG " + line[:160])
    await c.disconnect()
    c = await connect()
    mark(f"second client: crash_report={(await c.device_capabilities()).crash_report}")
    r3 = await c.crash_report()
    mark(f"REPORT3 flags={r3.flags!r}")
    await c.disconnect()
    if not offered:
        return "no crash report offered after the crash"
    if r.reason in (None, CrashReason.UNKNOWN) or r.pc == 0:
        return f"the report has reason {r.reason} and PC {r.pc:#010x}"
    return None


if err := asyncio.run(main()):
    print(f"FAIL: {err}")
    sys.exit(1)
print("PASS")
