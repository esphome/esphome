"""A UDP datagram wakes the main loop at once, and sends before the sockets open are logged."""

from __future__ import annotations

import asyncio
import contextlib
import socket
import time

import pytest

from .types import RunCompiledFunction


@pytest.mark.asyncio
async def test_socket_wake_gate_udp(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    # Hold the port with SO_REUSEADDR so nothing else claims it while the firmware builds; the
    # device's listen socket sets SO_REUSEADDR too and binds the wildcard address alongside until
    # the hold is released. Binding the hold to loopback keeps that legal on macOS as well.
    hold_sock: socket.socket | None = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    hold_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    hold_sock.bind(("127.0.0.1", 0))
    listen_port = hold_sock.getsockname()[1]

    config = yaml_config.replace("UDP_LISTEN_PORT_PLACEHOLDER", str(listen_port))

    loop = asyncio.get_running_loop()
    boot_done: asyncio.Future[None] = loop.create_future()
    sockets_ready: asyncio.Future[None] = loop.create_future()
    pong: asyncio.Future[None] = loop.create_future()
    events: list[str] = []

    def on_log_line(line: str) -> None:
        if "Sockets not open yet" in line:
            events.append("dropped")
        elif "Sockets ready" in line:
            events.append("ready")
            if not sockets_ready.done():
                sockets_ready.set_result(None)
        elif "BOOT_DONE" in line and not boot_done.done():
            boot_done.set_result(None)
        elif "PONG" in line and not pong.done():
            pong.set_result(None)

    try:
        async with run_compiled(config, line_callback=on_log_line):
            try:
                await asyncio.wait_for(boot_done, timeout=15.0)
            except TimeoutError:
                pytest.fail("BOOT_DONE never appeared")
            try:
                await asyncio.wait_for(sockets_ready, timeout=15.0)
            except TimeoutError:
                pytest.fail("Sockets ready never appeared")
            # The early write ran before the sockets existed and must be dropped with a log line
            assert events == ["dropped", "ready"]

            hold_sock.close()
            hold_sock = None

            # No API client is connected, so this datagram is the only thing that can wake select()
            send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            t_send = time.monotonic()
            try:
                send_sock.sendto(b"PING", ("127.0.0.1", listen_port))
            finally:
                send_sock.close()

            try:
                await asyncio.wait_for(pong, timeout=5.0)
            except TimeoutError:
                pytest.fail("PONG never appeared")

            elapsed_ms = (time.monotonic() - t_send) * 1000
            # Without the wake the receive waits for loop_interval_ (2000 ms); 500 ms leaves CI headroom
            assert elapsed_ms < 500, (
                f"UDP receive took {elapsed_ms:.0f}ms with loop_interval_=2000ms"
            )
    finally:
        if hold_sock is not None:
            with contextlib.suppress(OSError):
                hold_sock.close()
