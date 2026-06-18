"""Test that a UDP socket receive opens the component-phase gate immediately.

Regression test for socket_loop_monitored() + ready() guard in
udp_component.cpp. The UDP listen socket is registered with the host
select() loop via socket_loop_monitored(). When a packet arrives,
select() returns early and wake_request_set() is called, opening the
component-phase gate. The ready() guard in loop() then allows the drain
to proceed on that same tick.

Without socket_loop_monitored: the loop does not wake on UDP data; the
packet sits in the kernel buffer until loop_interval_ expires (~2000ms).
Without the ready() guard: the socket is wake-registered but loop()
still calls read() unconditionally — wake benefit is still present but
the contract is violated vs. every other loop-monitored socket.
"""

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
    """UDP socket receive must open the component-phase gate immediately,
    even with loop_interval_ raised to 2000ms."""
    # Hold the port open with SO_REUSEADDR to prevent another process from
    # claiming it during compilation and binary startup. The device's listen
    # socket also sets SO_REUSEADDR, allowing it to bind the same port while
    # this socket is still open. The hold is released after BOOT_DONE, which
    # guarantees all setup() calls have completed and the device owns the port.
    hold_sock: socket.socket | None = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    hold_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    hold_sock.bind(("", 0))
    listen_port = hold_sock.getsockname()[1]

    config = yaml_config.replace("UDP_LISTEN_PORT_PLACEHOLDER", str(listen_port))

    loop = asyncio.get_running_loop()
    boot_done: asyncio.Future[None] = loop.create_future()
    pong: asyncio.Future[None] = loop.create_future()

    def on_log_line(line: str) -> None:
        if "BOOT_DONE" in line and not boot_done.done():
            boot_done.set_result(None)
        if "PONG" in line and not pong.done():
            pong.set_result(None)

    try:
        async with run_compiled(config, line_callback=on_log_line):
            try:
                await asyncio.wait_for(boot_done, timeout=15.0)
            except TimeoutError:
                pytest.fail("BOOT_DONE never appeared — device did not complete boot")

            # BOOT_DONE confirms on_boot(priority=-100) has run, meaning all
            # setup() calls are complete and the UDP listen socket is bound.
            # Release the hold — the device is now the sole owner of the port.
            hold_sock.close()
            hold_sock = None

            # Send a UDP packet and time how long until on_receive fires.
            # The packet arrives on the loop-monitored listen_socket_, which
            # wakes the select() loop and sets the component-phase gate flag.
            # ready() in loop() then allows the drain to proceed immediately.
            # No API client is connected; all select() wakeups come from this
            # UDP packet alone, so the timing assertion is unambiguous.
            send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            t_send = time.monotonic()
            try:
                send_sock.sendto(b"PING", ("127.0.0.1", listen_port))
            finally:
                send_sock.close()

            try:
                await asyncio.wait_for(pong, timeout=5.0)
            except TimeoutError:
                pytest.fail("PONG never appeared — on_receive did not fire")

            elapsed_ms = (time.monotonic() - t_send) * 1000
            # Without socket_loop_monitored the gate stays closed for up to
            # loop_interval_=2000ms. With the fix the gate opens on the next
            # tick; 500ms gives ample CI headroom.
            assert elapsed_ms < 500, (
                f"UDP receive took {elapsed_ms:.0f}ms with loop_interval_=2000ms — "
                f"expected < 500ms; without the wake-gate fix this would take up to 2000ms"
            )
    finally:
        if hold_sock is not None:
            with contextlib.suppress(OSError):
                hold_sock.close()
