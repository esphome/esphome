"""Test that a TCP socket receive opens the component-phase gate immediately.

Regression test for the wake-request flag not being set when select() returns
due to socket data on the host platform (wake_host.cpp wakeable_delay fix).

The API server's accepted connection sockets use accept_loop_monitored(), so
they are registered with the host select() loop. A service call from the Python
client arrives on that socket. Without the fix, select() returning early did not
set g_wake_requested, so Application::loop()'s Phase B gate stayed closed until
loop_interval_ expired. With the fix, the gate opens immediately.
"""

from __future__ import annotations

import asyncio
import time

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_socket_wake_gate_tcp(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """TCP socket receive must open the component-phase gate immediately,
    even with loop_interval_ raised to 2000ms."""
    loop = asyncio.get_running_loop()
    boot_done: asyncio.Future[None] = loop.create_future()
    pong: asyncio.Future[None] = loop.create_future()

    def on_log_line(line: str) -> None:
        if "BOOT_DONE" in line and not boot_done.done():
            boot_done.set_result(None)
        if "PONG" in line and not pong.done():
            pong.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "socket-wake-gate-tcp"

        try:
            await asyncio.wait_for(boot_done, timeout=15.0)
        except TimeoutError:
            pytest.fail("BOOT_DONE never appeared — device did not complete boot")

        _, services = await client.list_entities_services()
        ping_service = next((s for s in services if s.name == "ping"), None)
        assert ping_service is not None, "ping service not found"

        # Execute the service and time how long until PONG appears in logs.
        # The request bytes arrive on an accept_loop_monitored() TCP socket,
        # which is registered with the host select() loop.
        t_send = time.monotonic()
        await client.execute_service(ping_service, {})

        try:
            await asyncio.wait_for(pong, timeout=5.0)
        except TimeoutError:
            pytest.fail("PONG never appeared — service did not execute")

        elapsed_ms = (time.monotonic() - t_send) * 1000
        # Without the fix the gate stays closed for up to loop_interval_=2000ms.
        # With the fix the gate opens on the next tick; 500ms gives ample CI headroom.
        assert elapsed_ms < 500, (
            f"Service response took {elapsed_ms:.0f}ms with loop_interval_=2000ms — "
            f"expected < 500ms; without the wake-request fix this would take up to 2000ms"
        )
