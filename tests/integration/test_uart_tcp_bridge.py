"""Integration test for the uart_tcp bridge on host.

The UART bus is backed by a pty; pytest holds the controller side and connects
as the TCP client. Covers both transfer directions, the stale-byte discard
at every accept, and the drop plus client replacement path.
"""

from __future__ import annotations

import asyncio
import os

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_tcp_bridge(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory,
) -> None:
    server_port = unused_tcp_port_factory()
    controller_fd, device_fd = os.openpty()
    os.set_blocking(controller_fd, False)
    yaml_config = yaml_config.replace("port: 18126", f"port: {server_port}")
    yaml_config = yaml_config.replace("PTY_PATH", os.ttyname(device_fd))

    lines = LineWaiter()
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

    async def wait_log_count(needle: str, count: int, timeout: float = 15.0) -> None:
        async with asyncio.timeout(timeout):
            while sum(needle in line for line in lines.lines) < count:
                await asyncio.sleep(0.05)

    loop.add_reader(controller_fd, on_controller_readable)
    try:
        async with (
            run_compiled(yaml_config, line_callback=lines.callback),
            api_client_connected() as client,
        ):
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "uart-tcp-bridge-test"
            await lines.wait_for("Listening on")

            # Bytes written before any client connects must never reach one.
            os.write(controller_fd, b"STALE")
            await asyncio.sleep(0.2)

            reader, writer = await asyncio.open_connection("127.0.0.1", server_port)
            await wait_log_count("Client connected", 1)
            os.write(controller_fd, b"live!")
            assert await asyncio.wait_for(reader.readexactly(5), 10) == b"live!", (
                "First bytes to the client were not the live payload"
            )
            writer.write(b"down1")
            await writer.drain()
            assert await read_uart(5) == b"down1"

            # Drop the client; bytes while no client is connected are discarded
            # when the next one is accepted.
            writer.close()
            await lines.wait_for("Connection lost")
            os.write(controller_fd, b"gap")
            await asyncio.sleep(0.2)

            reader, writer = await asyncio.open_connection("127.0.0.1", server_port)
            await wait_log_count("Client connected", 2)
            os.write(controller_fd, b"live2")
            assert await asyncio.wait_for(reader.readexactly(5), 10) == b"live2", (
                "Second client received stale bytes from the gap"
            )
            writer.write(b"down2")
            await writer.drain()
            assert await read_uart(5) == b"down2"
            writer.close()
    finally:
        loop.remove_reader(controller_fd)
        os.close(controller_fd)
        os.close(device_fd)
