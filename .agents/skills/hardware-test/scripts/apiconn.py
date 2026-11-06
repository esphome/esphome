"""Shared helpers for the API drivers: connect with retries, and the text of an API log line.

Import it with this directory on sys.path (the drivers add their parent directory, runners set PYTHONPATH).
"""

import asyncio
import os
import re
import sys

from aioesphomeapi import APIClient

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


async def connect(host: str, tries: int = 30, timeout: float = 10) -> APIClient:
    """Connect and log in, retrying once a second; exits after TRIES failures. API_KEY in the env selects Noise."""
    for _ in range(tries):
        c = APIClient(host, 6053, None, noise_psk=os.environ.get("API_KEY"))
        try:
            await asyncio.wait_for(c.connect(login=True), timeout)
        except Exception:  # noqa: BLE001
            await asyncio.sleep(1)
        else:
            return c
    sys.exit(f"no API connection to {host}")


def log_text(msg) -> str:
    """The message of a subscribe_logs callback as text, without ANSI colour codes."""
    return ANSI.sub("", msg.message.decode(errors="replace"))
