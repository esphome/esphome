"""Integration test for a tcp_uart server with two connections on host.

Two pytest clients connect to one port. The firmware echoes every byte, and
each client gets back only its own bytes, one turn after the other.
"""

from __future__ import annotations

import asyncio

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_tcp_uart_connections(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory,
) -> None:
    port = unused_tcp_port_factory()
    yaml_config = yaml_config.replace("port: 18128", f"port: {port}")

    lines = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=lines.callback),
        api_client_connected() as client,
    ):
        assert (await client.device_info()).name == "tcp-uart-connections"
        await lines.wait_for(f"Listening on {port}")

        reader_a, writer_a = await asyncio.open_connection("127.0.0.1", port)
        await lines.wait_for("Client connected from 127.0.0.1")
        reader_b, writer_b = await asyncio.open_connection("127.0.0.1", port)
        # wait_for() would match A's line again, so count the lines.
        async with asyncio.timeout(10):
            while sum("Client connected" in line for line in lines.lines) < 2:
                await asyncio.sleep(0.05)

        # Both ask at the same time; B is answered after A's turn ends.
        writer_a.write(b"from A")
        writer_b.write(b"from B")
        await writer_a.drain()
        await writer_b.drain()
        assert await asyncio.wait_for(reader_a.readexactly(6), 10) == b"from A"
        assert await asyncio.wait_for(reader_b.readexactly(6), 10) == b"from B"

        # A third client takes the place of the quieter one.
        reader_c, writer_c = await asyncio.open_connection("127.0.0.1", port)
        await lines.wait_for("All 2 connections in use, closing the quietest")
        writer_c.write(b"from C")
        await writer_c.drain()
        assert await asyncio.wait_for(reader_c.readexactly(6), 10) == b"from C"

        for writer in (writer_a, writer_b, writer_c):
            writer.close()
