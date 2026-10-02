"""Integration test for the tcp_uart bridge on host.

Pytest runs a real TCP server; a lambda echoes UART bytes back to the socket.
Covers the UART read and write paths, flush results for a live and a dropped
link, the offline drop warning and the automatic reconnect.
"""

from __future__ import annotations

import asyncio
import contextlib

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction

GREETING = b"hi"
RECONNECT_GREETING = b"yo"


async def _wait(event: asyncio.Event, timeout: float, message: str) -> None:
    try:
        await asyncio.wait_for(event.wait(), timeout)
    except TimeoutError:
        pytest.fail(message)


@pytest.mark.asyncio
async def test_tcp_uart_bridge(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory,
) -> None:
    server_port = unused_tcp_port_factory()
    yaml_config = yaml_config.replace("port: 18125", f"port: {server_port}")

    lines = LineWaiter()
    echo_ok = asyncio.Event()
    service_byte_ok = asyncio.Event()
    reconnect_echo_ok = asyncio.Event()
    sessions = 0

    async def handle(
        reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        nonlocal sessions
        sessions += 1
        if sessions == 1:
            writer.write(GREETING)
            await writer.drain()
            with contextlib.suppress(TimeoutError, asyncio.IncompleteReadError):
                if (
                    await asyncio.wait_for(reader.readexactly(len(GREETING)), 10)
                    == GREETING
                ):
                    echo_ok.set()
                if await asyncio.wait_for(reader.readexactly(1), 10) == b"X":
                    service_byte_ok.set()
            writer.close()
            return
        writer.write(RECONNECT_GREETING)
        await writer.drain()
        with contextlib.suppress(TimeoutError, asyncio.IncompleteReadError):
            if (
                await asyncio.wait_for(reader.readexactly(len(RECONNECT_GREETING)), 10)
                == RECONNECT_GREETING
            ):
                reconnect_echo_ok.set()

    server = await asyncio.start_server(handle, "127.0.0.1", server_port)
    try:
        async with (
            run_compiled(yaml_config, line_callback=lines.callback),
            api_client_connected() as client,
        ):
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "tcp-uart-bridge-test"
            _, services = await client.list_entities_services()
            send_byte = next(s for s in services if s.name == "send_byte")

            await _wait(echo_ok, 15.0, "UART echo through the bridge never arrived")

            await client.execute_service(send_byte, {})
            await _wait(service_byte_ok, 10.0, "Service byte never reached the server")
            await lines.wait_for("Flush result 0")

            # The server closed session one; a write while down must warn and
            # a flush on the down link must report FAILED (2).
            await lines.wait_for("Connection lost")
            await client.execute_service(send_byte, {})
            await lines.wait_for("Not connected, dropped")
            await lines.wait_for("Flush result 2")

            await _wait(
                reconnect_echo_ok, 15.0, "Bridge did not reconnect and echo again"
            )
    finally:
        server.close()
        await server.wait_closed()
