"""Integration test for UDP over IPv6 with the default dual-stack listen socket."""

from __future__ import annotations

import asyncio
import socket

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


def _free_port(family: socket.AddressFamily, host: str) -> int:
    with socket.socket(family, socket.SOCK_DGRAM) as sock:
        sock.bind((host, 0))
        return sock.getsockname()[1]


@pytest.mark.asyncio
async def test_udp_ipv6_unicast(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """IPv6 and IPv4 datagrams both reach the listen socket, and replies go out over IPv6."""
    if not socket.has_ipv6:
        pytest.skip("Host has no IPv6 support")
    try:
        reply_sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
        reply_sock.bind(("::1", 0))
    except OSError:
        pytest.skip("Host has no IPv6 loopback")
    reply_sock.setblocking(False)
    reply_port = reply_sock.getsockname()[1]
    listen_port = _free_port(socket.AF_INET6, "::")
    config = yaml_config.replace(
        "UDP_LISTEN_PORT_PLACEHOLDER", str(listen_port)
    ).replace("UDP_REPLY_PORT_PLACEHOLDER", str(reply_port))

    loop = asyncio.get_running_loop()
    try:
        async with (
            run_compiled(config),
            api_client_connected() as client,
        ):
            assert await client.device_info() is not None

            for family, host in (
                (socket.AF_INET6, "::1"),
                # An IPv4 sender reaches the IPv6 socket as a mapped address
                (socket.AF_INET, "127.0.0.1"),
            ):
                with socket.socket(family, socket.SOCK_DGRAM) as send_sock:
                    send_sock.sendto(b"PING", (host, listen_port))
                data = await wait_or_fail(
                    loop.sock_recv(reply_sock, 64),
                    5.0,
                    f"No reply over IPv6 to a PING sent to {host}",
                )
                assert data == b"PONG"
    finally:
        reply_sock.close()
