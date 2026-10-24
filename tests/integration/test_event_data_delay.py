"""An on_event that waits keeps the values of its own event."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_event_data_delay(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Two events in a row: the second replaces the stored values while the first waits 100 ms."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        _, services = await client.list_entities_services()
        await client.execute_service(require_service(services, "turn_twice"), {})

        await waiter.wait_for("later rotate_left 2", timeout=5.0)
        later = [line for line in waiter.lines if "later rotate_left" in line]
        assert "later rotate_left 1 first" in later[0]
        assert "later rotate_left 2 second" in later[1]
