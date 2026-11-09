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

import time

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_socket_wake_gate_tcp(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """TCP socket receive must open the component-phase gate immediately,
    even with loop_interval_ raised to 2000ms."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "socket-wake-gate-tcp")

        await waiter.wait_for("BOOT_DONE", timeout=15.0)

        _, services = await client.list_entities_services()
        ping_service = require_service(services, "ping")

        # Execute the service and time how long until PONG appears in logs.
        # The request bytes arrive on an accept_loop_monitored() TCP socket,
        # which is registered with the host select() loop.
        t_send = time.monotonic()
        await client.execute_service(ping_service, {})

        await waiter.wait_for("PONG", timeout=5.0)

        elapsed_ms = (time.monotonic() - t_send) * 1000
        # Without the fix the gate stays closed for up to loop_interval_=2000ms.
        # With the fix the gate opens on the next tick; 500ms gives ample CI headroom.
        assert elapsed_ms < 500, (
            f"Service response took {elapsed_ms:.0f}ms with loop_interval_=2000ms — "
            f"expected < 500ms; without the wake-request fix this would take up to 2000ms"
        )
