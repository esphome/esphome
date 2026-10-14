"""Integration test for the UDP component's deferred multicast join."""

from __future__ import annotations

import asyncio
import socket

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

MULTICAST_GROUP = "239.1.2.3"


def _get_free_udp_port() -> int:
    """Get a free UDP port by binding to port 0 and releasing."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


@pytest.mark.asyncio
async def test_udp_multicast_join(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The group is joined from the poll after setup, once, and then receives."""
    log_lines: list[str] = []
    joined_event = asyncio.Event()
    receive_event = asyncio.Event()

    def on_log_line(line: str) -> None:
        log_lines.append(line)
        if f"Joined multicast {MULTICAST_GROUP}" in line:
            joined_event.set()
        if "Received UDP:" in line:
            receive_event.set()

    listen_port = _get_free_udp_port()
    config = yaml_config.replace("UDP_LISTEN_PORT_PLACEHOLDER", str(listen_port))

    async with (
        run_compiled(config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "udp-multicast-test"

        try:
            await asyncio.wait_for(joined_event.wait(), timeout=5.0)
        except TimeoutError:
            pytest.fail(f"Multicast group was not joined. Log: {log_lines[-20:]}")

        # The poll cancels itself after the join, so a second poll period must not join again
        await asyncio.sleep(2.5)
        join_lines = [line for line in log_lines if "Joined multicast" in line]
        assert len(join_lines) == 1, f"Expected one join, got: {join_lines}"
        assert not any("IP_ADD_MEMBERSHIP" in line for line in log_lines), (
            f"Join reported a failure. Log: {log_lines[-20:]}"
        )

        send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            send_sock.sendto(b"MULTICAST_PAYLOAD", (MULTICAST_GROUP, listen_port))
        finally:
            send_sock.close()

        try:
            await asyncio.wait_for(receive_event.wait(), timeout=5.0)
        except TimeoutError:
            pytest.fail(f"Multicast datagram was not received. Log: {log_lines[-20:]}")
