"""Integration test for Host mode with sensor."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_with_sensor(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test Host mode with a sensor component."""
    # Write, compile and run the ESPHome device, then connect to API
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Subscribe to state changes
        states: dict[int, EntityState] = {}
        sensor_future: asyncio.Future[EntityState] = asyncio.Future()

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            # If this is our sensor with value 42.0, resolve the future
            if (
                hasattr(state, "state")
                and state.state == 42.0
                and not sensor_future.done()
            ):
                sensor_future.set_result(state)

        client.subscribe_states(on_state)

        # Wait for sensor with specific value (42.0) with timeout
        try:
            test_sensor_state = await asyncio.wait_for(sensor_future, timeout=5.0)
        except asyncio.TimeoutError:
            pytest.fail(
                f"Sensor with value 42.0 not received within 5 seconds. "
                f"Received states: {list(states.values())}"
            )

        # Verify the sensor state
        assert test_sensor_state.state == 42.0
        assert len(states) > 0, "No states received"
