"""Shared helpers for the voice API drivers: connect, log line cleanup, the microWakeWord buttons, host playback."""

import asyncio
import os
import re
import subprocess

from aioesphomeapi import APIClient

PLAY = os.environ.get("PLAY", "afplay").split()
_ANSI = re.compile(r"\x1b\[[0-9;]*m")


async def connect(host: str, key: str) -> APIClient:
    """Connect and log in; KEY "-" means plaintext."""
    cli = APIClient(host, 6053, None, noise_psk=None if key == "-" else key)
    await cli.connect(login=True)
    return cli


def log_text(msg: object) -> str:
    """A log message as plain text without colour codes."""
    return _ANSI.sub("", msg.message.decode(errors="replace"))


def mww_buttons(ents: list) -> tuple[object, object]:
    """The stop and start buttons from mww-buttons.yaml."""
    by_id = {e.object_id: e for e in ents}
    return by_id["hwtest_mww_stop"], by_id["hwtest_mww_start"]


async def play(path: str) -> None:
    """Play a clip on the host; raises when the player fails or hangs (afplay without audio permission)."""
    await asyncio.to_thread(subprocess.run, [*PLAY, path], check=True, timeout=30)
