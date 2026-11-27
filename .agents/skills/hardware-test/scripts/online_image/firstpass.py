"""Reboot the DUT before each fetch and report the longest online_image loop pass of that one fetch.
usage: firstpass.py HOST KEY BASEURL LABEL IMAGE...
The component warning prints only on a new maximum, so a fresh boot per image gives that image's own maximum.
"""

import asyncio
import contextlib
import re
import sys
import time

from aioesphomeapi import (
    APIClient,
    ButtonInfo,
    InvalidAuthAPIError,
    InvalidEncryptionKeyAPIError,
    LogLevel,
    RequiresEncryptionAPIError,
    UserService,
)

HOST, KEY, BASE, LABEL = sys.argv[1:5]
IMAGES = sys.argv[5:]
TOOK = re.compile(r"online_image\.image took a long time for an operation \((\d+) ms\)")
OUTCOME = re.compile(r"download (finished|error): img\b")


async def connect() -> tuple[APIClient, dict, int]:
    last: Exception | None = None
    for _ in range(30):
        c = APIClient(HOST, 6053, None, noise_psk=KEY)
        try:
            await c.connect(login=True)
            break
        except (
            InvalidAuthAPIError,
            InvalidEncryptionKeyAPIError,
            RequiresEncryptionAPIError,
        ):
            raise  # a wrong key never fixes itself
        except Exception as exc:  # noqa: BLE001
            last = exc
            await asyncio.sleep(2)
    else:
        raise RuntimeError(f"device did not come back (last error {last!r})")
    ents, services = await c.list_entities_services()
    reboot = next(
        (e.key for e in ents if isinstance(e, ButtonInfo) and e.name == "Reboot"), None
    )
    if reboot is None:
        raise RuntimeError("the device has no button named Reboot")
    return c, {s.name: s for s in services if isinstance(s, UserService)}, reboot


async def rebooted(c: APIClient, reboot: int) -> bool:
    """Press Reboot and wait up to 15 s for the API connection to drop."""
    c.button_command(reboot)
    end = time.monotonic() + 15
    while time.monotonic() < end:
        try:
            await asyncio.wait_for(c.device_info(), 1.0)
        except TimeoutError:
            continue  # a slow reply is not a reboot; only a closed connection counts
        except Exception:  # noqa: BLE001
            return True
        await asyncio.sleep(0.2)
    return False


async def main() -> int:
    failed = []
    for img in IMAGES:
        c, actions, reboot = await connect()
        ok = await rebooted(c, reboot)
        with contextlib.suppress(Exception):
            await c.disconnect()
        if not ok:
            print(f"{LABEL} {img:22} SKIPPED: no reboot within 15 s", flush=True)
            failed.append(img)
            continue
        await asyncio.sleep(8)
        c, actions, reboot = await connect()
        lines: list[str] = []
        c.subscribe_logs(
            lambda m, lines=lines: lines.append(m.message.decode(errors="replace")),
            log_level=LogLevel.LOG_LEVEL_DEBUG,
        )
        await asyncio.sleep(3)
        lines.clear()
        t0 = time.monotonic()
        await c.execute_service(actions["fetch"], {"url": f"{BASE}/{img}"})
        outcome = None
        while time.monotonic() - t0 < 60 and outcome is None:
            await asyncio.sleep(0.2)
            outcome = next(
                (m.group(1) for ln in lines if (m := OUTCOME.search(ln))), None
            )
        took = time.monotonic() - t0
        await asyncio.sleep(1)
        passes = [int(m.group(1)) for ln in lines if (m := TOOK.search(ln))]
        print(
            f"{LABEL} {img:22} {outcome or 'none':9} {took:5.1f}s longest pass {max(passes) if passes else '<50'} ms {passes}",
            flush=True,
        )
        if (
            outcome != "finished"
        ):  # a decode timing is only meaningful for a complete image
            failed.append(img)
        await c.disconnect()
    if failed:
        print(f"{LABEL} not finished: {', '.join(failed)}", flush=True)
    return 1 if failed else 0


sys.exit(asyncio.run(main()))
