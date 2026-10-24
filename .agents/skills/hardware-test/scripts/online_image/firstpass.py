"""Reboot the DUT before each fetch and report the longest online_image loop pass of that one fetch.
usage: firstpass.py HOST KEY BASEURL LABEL IMAGE...
The component warning prints only on a new maximum, so a fresh boot per image gives that image's own maximum.
"""

import asyncio
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


async def main() -> None:
    for img in IMAGES:
        c, actions, reboot = await connect()
        c.button_command(reboot)
        await asyncio.sleep(1)
        await c.disconnect()
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
        await c.disconnect()


asyncio.run(main())
