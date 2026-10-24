"""Integration test for the template time platform."""

from __future__ import annotations

import pytest

from .state_utils import SensorTracker, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction

# Timestamps are exposed as text sensors (rather than 32-bit float sensor states,
# which cannot represent a UNIX epoch exactly) so the exact value can be checked.
EXPECTED_STATES = {
    "fixed_timestamp": "1700000000",
    "offset_timestamp": "1700003600",
    "empty_time_valid": "invalid",
}


@pytest.mark.asyncio
async def test_template_time(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Verify the template time platform evaluates its lambda on demand."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()

        tracker = SensorTracker(list(EXPECTED_STATES))
        tracker.map_entities(entities)
        futures = tracker.expect_all(EXPECTED_STATES)
        await subscribe_after_initial_states(
            client, entities, on_state=tracker.on_state
        )
        await tracker.await_all(futures, timeout=3.0)
