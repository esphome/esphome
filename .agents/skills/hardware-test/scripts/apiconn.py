"""Shared helpers for the API drivers: connect with retries, and the text of an API log line.

Import it with this directory on sys.path (the drivers add their parent directory, runners set PYTHONPATH).
"""

import asyncio
from collections.abc import Awaitable, Callable
import contextlib
import os
import re
import sys

from aioesphomeapi import (
    APIClient,
    EncryptionPlaintextAPIError,
    InvalidAuthAPIError,
    InvalidEncryptionKeyAPIError,
    RequiresEncryptionAPIError,
)

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
# a wrong key or a plaintext/Noise mismatch does not go away with a retry
FATAL = (
    EncryptionPlaintextAPIError,
    InvalidAuthAPIError,
    InvalidEncryptionKeyAPIError,
    RequiresEncryptionAPIError,
)


async def connect(
    host: str,
    tries: int = 30,
    timeout: float = 10,
    port: int = 6053,
    on_stop: Callable[[bool], Awaitable[None]] | None = None,
) -> APIClient:
    """Connect and log in, retrying once a second; exits after TRIES failures with the last error.

    API_KEY in the env selects Noise; unset, empty or - is plaintext. ON_STOP is called when the connection ends.
    """
    key = os.environ.get("API_KEY")
    last: BaseException | None = None
    for _ in range(tries):
        c = APIClient(
            host, port, None, noise_psk=None if key in (None, "", "-") else key
        )
        try:
            await asyncio.wait_for(c.connect(on_stop=on_stop, login=True), timeout)
        except FATAL as e:
            sys.exit(f"no API connection to {host}: {type(e).__name__}: {e}")
        except Exception as e:  # noqa: BLE001
            last = e
            # release the half-open socket so retries do not use up the device's API slots
            with contextlib.suppress(Exception):
                await c.disconnect(force=True)
            await asyncio.sleep(1)
        else:
            return c
    sys.exit(
        f"no API connection to {host} after {tries} tries: {type(last).__name__}: {last}"
    )


def log_text(msg) -> str:
    """The message of a subscribe_logs callback as text, without ANSI colour codes."""
    return ANSI.sub("", msg.message.decode(errors="replace"))


def stop_event() -> tuple[asyncio.Event, Callable[[bool], Awaitable[None]]]:
    """An Event and an on_stop callback for connect() that sets it when the connection ends."""
    ev = asyncio.Event()

    async def on_stop(expected: bool) -> None:
        ev.set()

    return ev, on_stop


async def ble_connect(
    cli: APIClient,
    addr: int,
    on_lost: Callable[[int], None] | None = None,
    advert_timeout: float = 60,
    timeout: float = 30,
) -> tuple[bool, int, int, Callable[[], None]]:
    """Wait for an advert from ADDR, then connect through the proxy without a cache (like Home Assistant).

    Returns (connected, mtu, error, cancel); a missing advert or a connect timeout gives (False, 0, -1, cancel), so
    callers can retry. ON_LOST(error) is called when the link drops later.
    """
    seen = asyncio.Event()
    unsub = cli.subscribe_bluetooth_le_raw_advertisements(
        lambda m: (
            seen.set() if any(a.address == addr for a in m.advertisements) else None
        )
    )
    try:
        await asyncio.wait_for(seen.wait(), advert_timeout)
    except TimeoutError:
        return False, 0, -1, lambda: None
    finally:
        unsub()
    first: asyncio.Queue = asyncio.Queue()
    pending = True

    def on_state(connected: bool, mtu: int, err: int) -> None:
        nonlocal pending
        if pending:
            pending = False
            first.put_nowait((connected, mtu, err))
        elif not connected and on_lost is not None:
            on_lost(err)

    cancel = await cli.bluetooth_device_connect(
        addr,
        on_state,
        timeout=timeout,
        address_type=0,
        feature_flags=0xFF,
        has_cache=False,
    )
    try:
        connected, mtu, err = await asyncio.wait_for(first.get(), timeout + 5)
    except TimeoutError:
        return False, 0, -1, cancel
    return connected, mtu, err, cancel


async def enable_notify(
    cli: APIClient, addr: int, ch, on_notify: Callable[[int, bytearray], None]
) -> bool:
    """Start notify on characteristic CH and write its first CCCD (2902) like Home Assistant.

    Returns False when the characteristic lists no CCCD.
    """
    await cli.bluetooth_gatt_start_notify(addr, ch.handle, on_notify)
    cccd = next(
        (d for d in ch.descriptors if d.uuid.lower().startswith("00002902")), None
    )
    if cccd is None:
        return False
    await cli.bluetooth_gatt_write_descriptor(addr, cccd.handle, b"\x01\x00")
    return True
