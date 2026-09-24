"""Integration test for API batch_delay: 0 with rapid state transitions."""

from __future__ import annotations

import asyncio
import time

from aioesphomeapi import BinarySensorInfo, BinarySensorState, EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_batch_delay_zero_rapid_transitions(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that rapid binary sensor transitions are preserved with batch_delay: 0ms."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Track state changes
        state_changes: list[tuple[bool, float]] = []

        def on_state(state: EntityState) -> None:
            """Track state changes with timestamps."""
            if isinstance(state, BinarySensorState):
                state_changes.append((state.state, time.monotonic()))

        # Subscribe to state changes
        client.subscribe_states(on_state)

        # Wait for entity info
        entity_info, _ = await client.list_entities_services()
        binary_sensors = [e for e in entity_info if isinstance(e, BinarySensorInfo)]
        assert len(binary_sensors) == 1, "Expected 1 binary sensor"

        # Collect states for 2 seconds
        await asyncio.sleep(2.1)

        # Count ON->OFF transitions
        on_off_count = 0
        for i in range(1, len(state_changes)):
            if state_changes[i - 1][0] and not state_changes[i][0]:  # ON to OFF
                on_off_count += 1

        # With batch_delay: 0, we should capture rapid transitions
        # The test timing can be variable in CI, so we're being conservative
        # We mainly want to verify that we capture multiple rapid transitions
        assert on_off_count >= 5, (
            f"Expected at least 5 ON->OFF transitions with batch_delay: 0ms, got {on_off_count}. "
            "Rapid transitions may have been lost."
        )

        # Also verify that state changes are happening frequently
        assert len(state_changes) >= 10, (
            f"Expected at least 10 state changes, got {len(state_changes)}"
        )
