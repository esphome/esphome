"""A host build must survive a write to a peer that has closed the connection.

The device writes twice before it reads the close; the second write must fail
with EPIPE and drop the link instead of SIGPIPE killing the process.
"""

from __future__ import annotations

import asyncio
from collections.abc import Callable

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_write_after_peer_close(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory: Callable[[], int],
) -> None:
    port = unused_tcp_port_factory()
    yaml_config = yaml_config.replace("port: 18126", f"port: {port}")

    lines = LineWaiter()
    peers: asyncio.Queue[asyncio.StreamWriter] = asyncio.Queue()

    async def handle(_: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        peers.put_nowait(writer)

    server = await asyncio.start_server(handle, "127.0.0.1", port)
    try:
        async with (
            run_compiled(yaml_config, line_callback=lines.callback),
            api_client_connected() as client,
        ):
            peer = await asyncio.wait_for(peers.get(), 15.0)
            _, services = await client.list_entities_services()
            write_twice = next(s for s in services if s.name == "write_twice")

            await client.execute_service(write_twice, {})
            await lines.wait_for("Holding the loop")
            peer.close()
            await peer.wait_closed()

            # EPIPE (32 on Linux and macOS) must come back as an errno.
            await lines.wait_for("Connection lost: 32")
            assert await client.device_info() is not None
    finally:
        server.close()
        await server.wait_closed()
