"""Integration test for an encrypted uart_tcp server and tcp_uart client on host: pty
bytes echoed over each new session. The wrong key and the plaintext peer run through
the same stream in the tcp_uart test."""

from __future__ import annotations

import asyncio
from collections.abc import Callable
import os
import pathlib

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_tcp_noise(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory: Callable[[], int],
) -> None:
    bridge_port = unused_tcp_port_factory()
    controller_fd, device_fd = os.openpty()
    os.set_blocking(controller_fd, False)
    # uart's validate_port wants a two segment device path.
    pty_link = pathlib.Path(f"/tmp/uart-tcp-noise-pty-{os.getpid()}")
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
        yaml_config = yaml_config.replace("port: 18133", f"port: {bridge_port}")
        yaml_config = yaml_config.replace("PTY_PATH", str(pty_link))
        loop.add_reader(controller_fd, on_controller_readable)
        async with (
            run_compiled(yaml_config, line_callback=lines.callback),
            api_client_connected() as client,
        ):
            assert (await client.device_info()).name == "uart-tcp-noise-test"

            # Both ends of the bridge report the session.
            await lines.wait_count("Session encrypted", 2, timeout=15)

            # Pty to the uart_tcp server, over both encrypted legs to the echo and back.
            # One write stays below the pty's input queue, which takes about 1 KiB at once.
            data = bytes(range(256)) * 2
            assert os.write(controller_fd, data) == len(data)
            assert await read_uart(len(data)) == data

            # The quiet session ends; the client connects again with a new
            # handshake, and the bytes cross again.
            await lines.wait_for("Idle for")
            first_close = next(
                i for i, line in enumerate(lines.lines) if "Idle for" in line
            )
            # Until the first idle close only the bridge had a session: one
            # line per end.
            before_close = lines.lines[:first_close]
            assert sum("Session encrypted" in line for line in before_close) == 2
            await lines.wait_count("Session encrypted", 4, timeout=15)
            assert os.write(controller_fd, data) == len(data)
            assert await read_uart(len(data)) == data
    finally:
        loop.remove_reader(controller_fd)
        os.close(controller_fd)
        os.close(device_fd)
        pty_link.unlink(missing_ok=True)
