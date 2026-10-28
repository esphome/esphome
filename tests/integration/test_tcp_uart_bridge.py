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
from .state_utils import assert_device_name, require_service
from .tcp_utils import tcp_server
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

GREETING = b"hi"
RECONNECT_GREETING = b"yo"


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

    async with (
        tcp_server(handle, server_port),
        run_compiled(yaml_config, line_callback=lines.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "tcp-uart-bridge-test")
        _, services = await client.list_entities_services()
        send_byte = require_service(services, "send_byte")

        await wait_or_fail(echo_ok, 15.0, "UART echo through the bridge never arrived")

        await client.execute_service(send_byte, {})
        await wait_or_fail(
            service_byte_ok, 10.0, "Service byte never reached the server"
        )
        await lines.wait_for("Flush result 0")

        # The server closed session one; a write while down must warn and
        # a flush on the down link must report FAILED (2).
        await lines.wait_for("Connection lost")
        await client.execute_service(send_byte, {})
        await lines.wait_for("Not connected, dropped")
        await lines.wait_for("Flush result 2")

        await wait_or_fail(
            reconnect_echo_ok, 15.0, "Bridge did not reconnect and echo again"
        )
