"""Press one crash button after another on an RTL8720C test build and keep the decoded API logs of each next boot.
Usage: rtl_drive.py HOST OUTDIR TREE YAML BUTTON...   (button names from templates/rtl8720c-crash-buttons-dut.yaml)
Prints PRESS lines with the wall clock time, so resets can be timed against a serial capture taken with --ms.
Each case waits for the board to answer on the API port again, then saves 15 s of `esphome logs` (run from TREE,
so the CLI decodes the addresses) as OUTDIR/caseN-BUTTON.txt. A board that does not come back within BOOT_DEADLINE_S
prints CASE N BUTTON NO BOOT and the run exits 1."""

import asyncio
import contextlib
import os
from pathlib import Path
import socket
import subprocess
import sys
import time

from aioesphomeapi import APIClient

host, out, tree, yaml = sys.argv[1:5]
buttons = sys.argv[5:]
BOOT_DEADLINE_S = 120


def port_open() -> bool:
    try:
        socket.create_connection((host, 6053), 1).close()
    except OSError:
        return False
    return True


def wait_for_port() -> bool:
    deadline = time.monotonic() + BOOT_DEADLINE_S
    while not port_open():
        if time.monotonic() > deadline:
            return False
        time.sleep(1)
    return True


async def button_keys() -> dict[str, int]:
    cli = APIClient(host, 6053, None)
    await cli.connect(login=True)
    try:
        entities, _ = await cli.list_entities_services()
    finally:
        await cli.disconnect()
    return {e.name: e.key for e in entities if type(e).__name__ == "ButtonInfo"}


async def press(name: str, key: int) -> None:
    cli = APIClient(host, 6053, None)
    await cli.connect(login=True)
    now = time.time()
    print(
        f"PRESS {name} at {time.strftime('%H:%M:%S', time.localtime(now))}.{int(now * 1000) % 1000:03d}",
        flush=True,
    )
    cli.button_command(key)
    await asyncio.sleep(0.5)
    # the board resets within about a second, so the disconnect reply may never come
    with contextlib.suppress(Exception):
        await asyncio.wait_for(cli.disconnect(), 2)


if not wait_for_port():
    sys.exit("NO BOOT before the first case")
# Check every name up front, so a typo in the last case does not waste the crashes before it
keys = asyncio.run(button_keys())
if missing := [b for b in buttons if b not in keys]:
    sys.exit(f"Unknown buttons {missing}; the board has {sorted(keys)}")

for i, name in enumerate(buttons):
    if not wait_for_port():
        print(f"CASE {i} {name} NO BOOT", flush=True)
        sys.exit(1)
    time.sleep(5)
    asyncio.run(press(name, keys[name]))
    time.sleep(25)  # reset, boot and Wi-Fi
    if not wait_for_port():
        print(f"CASE {i} {name} NO BOOT", flush=True)
        sys.exit(1)
    with (Path(out) / f"case{i}-{name.replace(' ', '_')}.txt").open(
        "w", encoding="utf-8"
    ) as fh:
        subprocess.run(
            [
                "timeout",
                "15",
                sys.executable,
                "-m",
                "esphome",
                "logs",
                yaml,
                "--device",
                host,
            ],
            cwd=tree,
            env={**os.environ, "PYTHONPATH": tree, "PYTHONUNBUFFERED": "1"},
            stdout=fh,
            stderr=subprocess.STDOUT,
            check=False,
        )
    print(f"CASE {i} {name} done", flush=True)
