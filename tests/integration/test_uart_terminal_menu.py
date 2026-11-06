"""Integration test for the uart_terminal display drawn through a pseudo terminal.

The device writes its frames to one end of a pty and the test reads the other: the first frame
clears the screen and shows the menu, unchanged frames are not resent, and a key sequence on the
UART moves the menu, which redraws at once. At 1200 baud a UART without a write-room query is paced
to one byte per loop, so the escape sequences and line breaks must go out in parts, and a key pressed
while a frame is on the wire must still be drawn once that frame is out, even with no periodic update.
"""

from __future__ import annotations

import asyncio
import contextlib
import os
import pathlib
import pty

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

CLEAR_HOME = b"\x1b[2J\x1b[H"
HOME = b"\x1b[H"
DOWN_KEY = b"\x1b[B"
THIRD_SELECTED = b" Second     \r\n>Third      "


def frames(data: bytes) -> list[bytes]:
    """Split the terminal output into frames, each starting with a cursor-home sequence."""
    parts = data.split(HOME)
    return [part for part in parts if part and part != b"\x1b[2J"]


@pytest.mark.asyncio
@pytest.mark.parametrize(
    ("baud_rate", "update_interval"),
    [(115200, "100ms"), (1200, "100ms"), (1200, "never")],
)
async def test_uart_terminal_menu(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    baud_rate: int,
    update_interval: str,
) -> None:
    received = bytearray()
    loop = asyncio.get_running_loop()
    controller_fd: int | None = None
    device_fd: int | None = None
    # The uart port validator takes paths of the /dev/ttyUSB0 shape; a pty sits at /dev/pts/N on Linux,
    # so the device end is reached through a short symlink
    port = pathlib.Path(f"/tmp/esphome-uart-terminal-{os.getpid()}")
    yaml_config = (
        yaml_config.replace("UART_PORT", str(port))
        .replace("baud_rate: 115200", f"baud_rate: {baud_rate}")
        .replace("update_interval: 100ms", f"update_interval: {update_interval}")
    )

    def on_readable() -> None:
        with contextlib.suppress(BlockingIOError):
            received.extend(os.read(controller_fd, 4096))

    async def wait_for(condition, what: str, timeout: float = 10.0) -> None:
        try:
            async with asyncio.timeout(timeout):
                while not condition():
                    await asyncio.sleep(0.02)
        except TimeoutError:
            pytest.fail(f"Timeout waiting for {what}; received {bytes(received)!r}")

    try:
        controller_fd, device_fd = pty.openpty()
        port.symlink_to(os.ttyname(device_fd))
        os.set_blocking(controller_fd, False)
        loop.add_reader(controller_fd, on_readable)
        async with run_compiled(yaml_config), api_client_connected():
            # The first frame clears the screen and draws the menu with the first item selected
            await wait_for(
                lambda: b">First      \r\n Second     " in received, "first frame"
            )
            assert received.startswith(CLEAR_HOME)

            # Nothing changes, so only the periodic full redraw (every tenth 100 ms update) goes out, or
            # nothing at all without periodic updates. At 1200 baud a frame takes about half a second, so the window starts and ends on a whole frame
            def frame_done() -> bool:
                return received.endswith(b" Second     ")

            await wait_for(frame_done, "end of the first frame")
            seen = len(received)
            await asyncio.sleep(2.5)
            await wait_for(frame_done, "end of an idle frame")
            idle = frames(bytes(received[seen:]))
            if update_interval == "never":
                assert not idle, idle
            else:
                assert 1 <= len(idle) <= 3, idle
            assert all(f.startswith(b">First") for f in idle), idle

            # Two down-arrow sequences in quick succession select the third item. The first one redraws
            # right away; at 1200 baud the second arrives while that frame is still on the wire, and its
            # update must run once the frame is out
            seen = len(received)
            os.write(controller_fd, DOWN_KEY)
            await asyncio.sleep(0.2)
            os.write(controller_fd, DOWN_KEY)
            await wait_for(
                lambda: frames(bytes(received[seen:]))[-1:] == [THIRD_SELECTED],
                "frame after two presses",
            )
    finally:
        port.unlink(missing_ok=True)
        if controller_fd is not None:
            loop.remove_reader(controller_fd)
            os.close(controller_fd)
        if device_fd is not None:
            os.close(device_fd)
