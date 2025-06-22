"""Integration test for many entities to test API batching."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_many_entities(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test API batching with many entities of different types."""
    # Write, compile and run the ESPHome device, then connect to API
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Subscribe to state changes
        states: dict[int, EntityState] = {}
        entity_count_future: asyncio.Future[int] = loop.create_future()

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            # When we have received states from a good number of entities, resolve the future
            if len(states) >= 50 and not entity_count_future.done():
                entity_count_future.set_result(len(states))

        client.subscribe_states(on_state)

        # Wait for states from at least 50 entities with timeout
        try:
            entity_count = await asyncio.wait_for(entity_count_future, timeout=10.0)
        except asyncio.TimeoutError:
            pytest.fail(
                f"Did not receive states from at least 50 entities within 10 seconds. "
                f"Received {len(states)} states: {list(states.keys())}"
            )

        # Verify we received a good number of entity states
        assert entity_count >= 50, f"Expected at least 50 entities, got {entity_count}"
        assert len(states) >= 50, f"Expected at least 50 states, got {len(states)}"

        # Verify we have different entity types by checking some expected values
        sensor_states = [
            s
            for s in states.values()
            if hasattr(s, "state") and isinstance(s.state, float)
        ]

        assert len(sensor_states) >= 50, (
            f"Expected at least 50 sensor states, got {len(sensor_states)}"
        )
