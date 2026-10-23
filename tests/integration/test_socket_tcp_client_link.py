"""Integration test for socket::TcpClientLink on host.

Pytest runs a real TCP server; the device echoes through the link.
Covers connect, read, write, a server-initiated drop, the reconnect and
that no bytes from the first session leak into the second.
"""

from __future__ import annotations

import asyncio
import contextlib

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

PAYLOAD = b"hello link"
SECOND_PAYLOAD = b"second session"


@pytest.mark.asyncio
async def test_socket_tcp_client_link(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory,
) -> None:
    server_port = unused_tcp_port_factory()
    yaml_config = yaml_config.replace("port: 18123", f"port: {server_port}")

    echoed: list[bytes] = []
    second_echoed: list[bytes] = []
    echo_done = asyncio.Event()
    reconnected = asyncio.Event()
    waiter = LineWaiter()
    link_down = waiter.expect("Link down")
    writers: list[asyncio.StreamWriter] = []

    async def handle(
        reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        writers.append(writer)
        if not echo_done.is_set():
            writer.write(PAYLOAD)
            await writer.drain()
            with contextlib.suppress(TimeoutError, asyncio.IncompleteReadError):
                echoed.append(
                    await asyncio.wait_for(reader.readexactly(len(PAYLOAD)), 10)
                )
            echo_done.set()
            # Drop the connection so the link has to reconnect.
            writer.close()
            return
        # Second session: the first bytes back must be this session's echo;
        # anything left over from the first session would arrive ahead of it.
        writer.write(SECOND_PAYLOAD)
        await writer.drain()
        with contextlib.suppress(TimeoutError, asyncio.IncompleteReadError):
            second_echoed.append(
                await asyncio.wait_for(reader.readexactly(len(SECOND_PAYLOAD)), 10)
            )
        reconnected.set()

    server = await asyncio.start_server(handle, "127.0.0.1", server_port)
    try:
        async with (
            run_compiled(yaml_config, line_callback=waiter.callback),
            api_client_connected() as client,
        ):
            await assert_device_name(client, "socket-tcp-client-link-test")

            await wait_or_fail(echo_done, 15.0, "Link never connected or echoed")
            assert echoed and echoed[0] == PAYLOAD, "Echo payload mismatch"

            await waiter.wait(link_down, 15.0, "Link never reported the drop")
            msg = "Link did not reconnect after the server dropped it"
            await wait_or_fail(reconnected, 15.0, msg)
            await waiter.wait_count("Link up", 2, 15.0, msg)
            assert second_echoed == [SECOND_PAYLOAD], (
                "Second session echo wrong; stale bytes from the first session?"
            )
    finally:
        # On Python 3.12 wait_closed() waits for every connection the handlers
        # left open; nothing else closes them once the device has exited.
        for writer in writers:
            writer.close()
        server.close()
        await server.wait_closed()
