"""Integration test for the uart_terminal display drawn through a pseudo terminal.

The device writes its frames to one end of a pty and the test reads the other: the first frame
clears the screen and shows the menu, unchanged frames are not resent, and a key sequence on the
UART moves the menu, which redraws at once.
"""

from __future__ import annotations

import asyncio
import contextlib
import os
import pty

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

CLEAR_HOME = b"\x1b[2J\x1b[H"
HOME = b"\x1b[H"
DOWN_KEY = b"\x1b[B"


def frames(data: bytes) -> list[bytes]:
    """Split the terminal output into frames, each starting with a cursor-home sequence."""
    parts = data.split(HOME)
    return [part for part in parts if part and part != b"\x1b[2J"]


@pytest.mark.asyncio
async def test_uart_terminal_menu(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    controller_fd, device_fd = pty.openpty()
    yaml_config = yaml_config.replace("UART_PORT", os.ttyname(device_fd))
    os.set_blocking(controller_fd, False)
    received = bytearray()

    loop = asyncio.get_running_loop()

    def on_readable() -> None:
        with contextlib.suppress(BlockingIOError):
            received.extend(os.read(controller_fd, 4096))

    loop.add_reader(controller_fd, on_readable)

    async def wait_for(condition, what: str, timeout: float = 10.0) -> None:
        try:
            async with asyncio.timeout(timeout):
                while not condition():
                    await asyncio.sleep(0.02)
        except TimeoutError:
            pytest.fail(f"Timeout waiting for {what}; received {bytes(received)!r}")

    try:
        async with run_compiled(yaml_config), api_client_connected():
            # The first frame clears the screen and draws the menu with the first item selected
            await wait_for(
                lambda: b">First      \r\n Second     " in received, "first frame"
            )
            assert received.startswith(CLEAR_HOME)

            # Nothing changes, so only the periodic full redraw (every tenth 100 ms update) goes out
            seen = len(received)
            await asyncio.sleep(2.5)
            idle = frames(bytes(received[seen:]))
            assert 1 <= len(idle) <= 3, idle
            assert all(f.startswith(b">First") for f in idle), idle

            # A down-arrow sequence on the UART selects the second item and redraws right away
            seen = len(received)
            os.write(controller_fd, DOWN_KEY)
            await wait_for(
                lambda: b" First      \r\n>Second     " in received[seen:],
                "moved frame",
            )
    finally:
        loop.remove_reader(controller_fd)
        os.close(controller_fd)
        os.close(device_fd)
