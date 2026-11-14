"""Shared helpers for the API drivers: connect with retries, and the text of an API log line.

Import it with this directory on sys.path (the drivers add their parent directory, runners set PYTHONPATH).
"""

import asyncio
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


async def connect(host: str, tries: int = 30, timeout: float = 10) -> APIClient:
    """Connect and log in, retrying once a second; exits after TRIES failures with the last error.

    API_KEY in the env selects Noise; unset, empty or - is plaintext.
    """
    key = os.environ.get("API_KEY")
    last: BaseException | None = None
    for _ in range(tries):
        c = APIClient(
            host, 6053, None, noise_psk=None if key in (None, "", "-") else key
        )
        try:
            await asyncio.wait_for(c.connect(login=True), timeout)
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
