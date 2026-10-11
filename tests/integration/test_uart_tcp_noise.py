"""Integration test for an encrypted uart_tcp server and tcp_uart client on host.

The uart_tcp server copies a pty-backed UART to an encrypted socket; a tcp_uart
client with the same key connects to it and a lambda echoes what it reads.
Bytes written to the pty come back through both encrypted legs. The server
closes a quiet session after its timeout, the client connects again, and the
next session carries the bytes too. A client with another key never gets a
session, and a plaintext peer is closed.
"""

from __future__ import annotations

import asyncio
from collections.abc import Callable
import contextlib
import os
import pathlib

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


def _count(lines: list[str], text: str) -> int:
    return sum(text in line for line in lines)


@pytest.mark.asyncio
async def test_uart_tcp_noise(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory: Callable[[], int],
) -> None:
    bridge_port = unused_tcp_port_factory()
    locked_port = unused_tcp_port_factory()
    controller_fd, device_fd = os.openpty()
    locked_controller_fd, locked_device_fd = os.openpty()
    os.set_blocking(controller_fd, False)
    # uart's validate_port wants a two segment device path.
    pty_link = pathlib.Path(f"/tmp/uart-tcp-noise-pty-{os.getpid()}")
    locked_pty_link = pathlib.Path(f"/tmp/uart-tcp-noise-locked-pty-{os.getpid()}")
    loop = asyncio.get_running_loop()
    uart_rx = bytearray()
    uart_rx_event = asyncio.Event()

    def on_controller_readable() -> None:
        try:
            chunk = os.read(controller_fd, 256)
        except BlockingIOError:
            return
        if chunk:
            uart_rx.extend(chunk)
            uart_rx_event.set()

    async def read_uart(count: int, timeout: float = 10.0) -> bytes:
        while len(uart_rx) < count:
            uart_rx_event.clear()
            await asyncio.wait_for(uart_rx_event.wait(), timeout)
        data = bytes(uart_rx[:count])
        del uart_rx[:count]
        return data

    lines = LineWaiter()
    try:
        pty_link.symlink_to(os.ttyname(device_fd))
        locked_pty_link.symlink_to(os.ttyname(locked_device_fd))
        yaml_config = yaml_config.replace("port: 18133", f"port: {bridge_port}")
        yaml_config = yaml_config.replace("port: 18134", f"port: {locked_port}")
        yaml_config = yaml_config.replace("LOCKED_PTY_PATH", str(locked_pty_link))
        yaml_config = yaml_config.replace("PTY_PATH", str(pty_link))
        loop.add_reader(controller_fd, on_controller_readable)
        async with (
            run_compiled(yaml_config, line_callback=lines.callback),
            api_client_connected() as client,
        ):
            assert (await client.device_info()).name == "uart-tcp-noise-test"

            # Both ends of the bridge report the session.
            async with asyncio.timeout(15):
                while _count(lines.lines, "Session encrypted") < 2:
                    await asyncio.sleep(0.05)

            # Pty to the uart_tcp server, over both encrypted legs to the echo and back.
            # One write stays below the pty's input queue, which takes about 1 KiB at once.
            data = bytes(range(256)) * 2
            assert os.write(controller_fd, data) == len(data)
            assert await read_uart(len(data)) == data

            # The other key fails on both ends.
            await lines.wait_for("Handshake MAC failure, check the key")
            await lines.wait_for("Peer rejected the handshake: Handshake MAC failure")

            # A plaintext peer is closed; it may wait for the other key's
            # attempt to end first.
            reader, writer = await asyncio.open_connection("127.0.0.1", locked_port)
            writer.write(b"AT\r\n")
            await writer.drain()
            with contextlib.suppress(ConnectionResetError):
                assert await asyncio.wait_for(reader.read(8), 10) == b""
            writer.close()
            await lines.wait_for("Bad frame")

            # The quiet session ends; the client connects again with a new
            # handshake, and the bytes cross again.
            await lines.wait_for("Idle for")
            first_close = next(
                i for i, line in enumerate(lines.lines) if "Idle for" in line
            )
            async with asyncio.timeout(15):
                while _count(lines.lines[first_close:], "Session encrypted") < 2:
                    await asyncio.sleep(0.05)
            assert os.write(controller_fd, data) == len(data)
            assert await read_uart(len(data)) == data

            # Until the first idle close only the bridge had a session: one
            # line per end.
            assert _count(lines.lines[:first_close], "Session encrypted") == 2
    finally:
        loop.remove_reader(controller_fd)
        for fd in (controller_fd, device_fd, locked_controller_fd, locked_device_fd):
            os.close(fd)
        pty_link.unlink(missing_ok=True)
        locked_pty_link.unlink(missing_ok=True)
