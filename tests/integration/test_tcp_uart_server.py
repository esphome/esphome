"""Integration test for a tcp_uart server on host.

Pytest connects as the TCP client. One server allows 127.0.0.1 and echoes.
The other allows only 192.0.2.1, so the same client is closed.
"""

from __future__ import annotations

import asyncio
import contextlib

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction

PAYLOAD = b"ping!"


@pytest.mark.asyncio
async def test_tcp_uart_server(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory,
) -> None:
    allowed_port = unused_tcp_port_factory()
    denied_port = unused_tcp_port_factory()
    yaml_config = yaml_config.replace("port: 18126", f"port: {allowed_port}")
    yaml_config = yaml_config.replace("port: 18127", f"port: {denied_port}")

    lines = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=lines.callback),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "tcp-uart-server-test"
        await lines.wait_for(f"Listening on {allowed_port}")
        await lines.wait_for(f"Listening on {denied_port}")

        reader, writer = await asyncio.open_connection("127.0.0.1", allowed_port)
        await lines.wait_for("Client connected from 127.0.0.1")
        writer.write(PAYLOAD)
        await writer.drain()
        assert await asyncio.wait_for(reader.readexactly(len(PAYLOAD)), 10) == PAYLOAD
        writer.close()

        denied_reader, denied_writer = await asyncio.open_connection(
            "127.0.0.1", denied_port
        )
        await lines.wait_for("Rejected 127.0.0.1")
        with contextlib.suppress(ConnectionResetError):
            assert await asyncio.wait_for(denied_reader.read(8), 10) == b""
        denied_writer.close()
