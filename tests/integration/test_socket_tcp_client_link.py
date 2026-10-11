"""Integration test for socket::TcpClientLink on host.

Pytest runs a real TCP server; the device echoes through the link.
Covers connect, read, write, a server-initiated drop, the reconnect and
that no bytes from the first session leak into the second.
"""

from __future__ import annotations

import asyncio
import contextlib

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

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
    link_down = asyncio.Event()
    second_link_up = asyncio.Event()
    link_up_count = 0

    def on_log_line(line: str) -> None:
        nonlocal link_up_count
        if "Link up" in line:
            link_up_count += 1
            if link_up_count >= 2:
                second_link_up.set()
        elif "Link down" in line:
            link_down.set()

    async def handle(
        reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
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
            run_compiled(yaml_config, line_callback=on_log_line),
            api_client_connected() as client,
        ):
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "socket-tcp-client-link-test"

            try:
                await asyncio.wait_for(echo_done.wait(), timeout=15.0)
            except TimeoutError:
                pytest.fail("Link never connected or echoed")
            assert echoed and echoed[0] == PAYLOAD, "Echo payload mismatch"

            try:
                await asyncio.wait_for(link_down.wait(), timeout=15.0)
            except TimeoutError:
                pytest.fail("Link never reported the drop")
            try:
                await asyncio.wait_for(reconnected.wait(), timeout=15.0)
                await asyncio.wait_for(second_link_up.wait(), timeout=15.0)
            except TimeoutError:
                pytest.fail("Link did not reconnect after the server dropped it")
            assert second_echoed == [SECOND_PAYLOAD], (
                "Second session echo wrong; stale bytes from the first session?"
            )
    finally:
        server.close()
        await server.wait_closed()
