"""Test that an incoming UDP datagram wakes the main loop at once."""

from __future__ import annotations

import socket
import time

import pytest

from .log_utils import LineWaiter
from .types import RunCompiledFunction


@pytest.mark.asyncio
async def test_socket_wake_gate_udp(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    """A datagram must be handled well before loop_interval_ (2000ms) expires."""
    # Hold the port with SO_REUSEADDR so nothing else claims it while the firmware builds. The
    # device's listen socket sets SO_REUSEADDR too and binds the wildcard address alongside it.
    hold_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    hold_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    hold_sock.bind(("127.0.0.1", 0))
    listen_port = hold_sock.getsockname()[1]
    config = yaml_config.replace("UDP_LISTEN_PORT_PLACEHOLDER", str(listen_port))

    waiter = LineWaiter()

    try:
        async with run_compiled(config, line_callback=waiter.callback):
            await waiter.wait_for("BOOT_DONE", timeout=15.0)

            # Datagrams go to the more specific bind, so release the hold before sending
            hold_sock.close()

            # No API client is connected, so this datagram is the only thing that can wake select()
            t_send = time.monotonic()
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as send_sock:
                send_sock.sendto(b"PING", ("127.0.0.1", listen_port))

            await waiter.wait_for("PONG", timeout=5.0)

            elapsed_ms = (time.monotonic() - t_send) * 1000
            # Without the wake the receive waits for loop_interval_; 500ms leaves CI headroom
            assert elapsed_ms < 500, (
                f"UDP receive took {elapsed_ms:.0f}ms with loop_interval_=2000ms"
            )
    finally:
        hold_sock.close()
