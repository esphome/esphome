"""A host build must survive a write to a peer that has closed the connection.

The device writes until the link drops; once the closed peer has answered with
a reset, a write must fail with EPIPE instead of SIGPIPE killing the process.
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
            action = next(s for s in services if s.name == "write_after_close")

            await client.execute_service(action, {"port": port})
            # The action opens a second connection and blocks the loop until it gets a byte.
            sync = await asyncio.wait_for(peers.get(), 15.0)
            peer.close()
            await peer.wait_closed()
            sync.write(b"g")
            sync.close()

            # EPIPE (32 on Linux and macOS) must come back as an errno.
            await lines.wait_for("Connection lost: 32")
            assert await client.device_info() is not None
    finally:
        server.close()
        await server.wait_closed()
