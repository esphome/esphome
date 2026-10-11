"""Integration test for an encrypted tcp_uart pair on host.

A tcp_uart server sends 512 bytes into each new session, and a lambda on the
tcp_uart client echoes them back. The server closes a quiet session after its
timeout, so the client connects again and the next session carries the bytes
too. A client with another key never gets a session, and a plaintext peer is
closed.
"""

from __future__ import annotations

import asyncio
from collections.abc import Callable
import contextlib

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


def _count(lines: list[str], text: str) -> int:
    return sum(text in line for line in lines)


@pytest.mark.asyncio
async def test_tcp_uart_noise(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory: Callable[[], int],
) -> None:
    pair_port = unused_tcp_port_factory()
    locked_port = unused_tcp_port_factory()
    sealed_port = unused_tcp_port_factory()
    yaml_config = yaml_config.replace("port: 18130", f"port: {pair_port}")
    yaml_config = yaml_config.replace("port: 18131", f"port: {locked_port}")
    yaml_config = yaml_config.replace("port: 18132", f"port: {sealed_port}")

    lines = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=lines.callback),
        api_client_connected() as client,
    ):
        assert (await client.device_info()).name == "tcp-uart-noise-test"

        # Server to client and back, over both encrypted legs.
        await lines.wait_for("Echo complete")

        # The other key: the server names the MAC failure and the client gets
        # the reason.
        await lines.wait_for("Handshake MAC failure, check the key")
        await lines.wait_for("Peer rejected the handshake: Handshake MAC failure")

        # A plaintext peer is closed.
        reader, writer = await asyncio.open_connection("127.0.0.1", sealed_port)
        writer.write(b"AT\r\n")
        await writer.drain()
        with contextlib.suppress(ConnectionResetError):
            assert await asyncio.wait_for(reader.read(8), 10) == b""
        writer.close()
        await lines.wait_for("Bad frame")

        # The quiet session ends; the client connects again with a new handshake.
        await lines.wait_for("Idle for")
        async with asyncio.timeout(15):
            while _count(lines.lines, "Echo complete") < 2:
                await asyncio.sleep(0.05)

        # Until the first idle close only the pair had a session: one line per end.
        first_close = next(
            i for i, line in enumerate(lines.lines) if "Idle for" in line
        )
        assert _count(lines.lines[:first_close], "Session encrypted") == 2
        assert _count(lines.lines, "Session encrypted") >= 4
        assert _count(lines.lines, "Echo mismatch") == 0
