"""Test sensor ring buffer filter functionality (window_size != send_every)."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, SensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_sensor_filters_ring_buffer(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that ring buffer filters (window_size != send_every) work correctly."""
    loop = asyncio.get_running_loop()

    # Track state changes for each sensor
    sensor_states: dict[str, list[float]] = {
        "sliding_min": [],
        "sliding_max": [],
        "sliding_median": [],
        "sliding_moving_avg": [],
    }

    # Futures to track when we receive expected values
    all_updates_received = loop.create_future()

    def on_state(state: EntityState) -> None:
        """Track sensor state updates."""
        if not isinstance(state, SensorState):
            return

        # Skip NaN values
        if state.missing_state:
            return

        # Get the sensor name from the key mapping
        sensor_name = key_to_sensor.get(state.key)
        if not sensor_name or sensor_name not in sensor_states:
            return

        sensor_states[sensor_name].append(state.state)

        # Check if we've received enough updates from all sensors
        # With send_every=2, send_first_at=1, we expect 5 outputs per sensor
        if (
            len(sensor_states["sliding_min"]) >= 5
            and len(sensor_states["sliding_max"]) >= 5
            and len(sensor_states["sliding_median"]) >= 5
            and len(sensor_states["sliding_moving_avg"]) >= 5
            and not all_updates_received.done()
        ):
            all_updates_received.set_result(True)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        # Get entities first to build key mapping
        entities, services = await client.list_entities_services()

        # Build key-to-sensor mapping
        key_to_sensor = build_key_to_entity_mapping(
            entities,
            [
                "sliding_min",
                "sliding_max",
                "sliding_median",
                "sliding_moving_avg",
            ],
        )

        # Set up initial state helper with all entities
        initial_state_helper = InitialStateHelper(entities)

        # Subscribe to state changes with wrapper
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for initial states to be sent before pressing button
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Find the publish button
        publish_button = next(
            (e for e in entities if "publish_values_button" in e.object_id.lower()),
            None,
        )
        assert publish_button is not None, "Publish Values Button not found"

        # Press the button to publish test values
        client.button_command(publish_button.key)

        # Wait for all sensors to receive their values
        try:
            await asyncio.wait_for(all_updates_received, timeout=10.0)
        except TimeoutError:
            # Provide detailed failure info
            pytest.fail(
                f"Timeout waiting for updates. Received states:\n"
                f"  min: {sensor_states['sliding_min']}\n"
                f"  max: {sensor_states['sliding_max']}\n"
                f"  median: {sensor_states['sliding_median']}\n"
                f"  moving_avg: {sensor_states['sliding_moving_avg']}"
            )

        # Verify we got 5 outputs per sensor (positions 1, 3, 5, 7, 9)
        assert len(sensor_states["sliding_min"]) == 5, (
            f"Min sensor should have 5 values, got {len(sensor_states['sliding_min'])}: {sensor_states['sliding_min']}"
        )
        assert len(sensor_states["sliding_max"]) == 5
        assert len(sensor_states["sliding_median"]) == 5
        assert len(sensor_states["sliding_moving_avg"]) == 5

        # Verify the values at each output position
        # Position 1: window=[1]
        assert sensor_states["sliding_min"][0] == pytest.approx(1.0)
        assert sensor_states["sliding_max"][0] == pytest.approx(1.0)
        assert sensor_states["sliding_median"][0] == pytest.approx(1.0)
        assert sensor_states["sliding_moving_avg"][0] == pytest.approx(1.0)

        # Position 3: window=[1,2,3]
        assert sensor_states["sliding_min"][1] == pytest.approx(1.0)
        assert sensor_states["sliding_max"][1] == pytest.approx(3.0)
        assert sensor_states["sliding_median"][1] == pytest.approx(2.0)
        assert sensor_states["sliding_moving_avg"][1] == pytest.approx(2.0)

        # Position 5: window=[1,2,3,4,5]
        assert sensor_states["sliding_min"][2] == pytest.approx(1.0)
        assert sensor_states["sliding_max"][2] == pytest.approx(5.0)
        assert sensor_states["sliding_median"][2] == pytest.approx(3.0)
        assert sensor_states["sliding_moving_avg"][2] == pytest.approx(3.0)

        # Position 7: window=[3,4,5,6,7] (ring buffer wrapped)
        assert sensor_states["sliding_min"][3] == pytest.approx(3.0)
        assert sensor_states["sliding_max"][3] == pytest.approx(7.0)
        assert sensor_states["sliding_median"][3] == pytest.approx(5.0)
        assert sensor_states["sliding_moving_avg"][3] == pytest.approx(5.0)

        # Position 9: window=[5,6,7,8,9] (ring buffer wrapped)
        assert sensor_states["sliding_min"][4] == pytest.approx(5.0)
        assert sensor_states["sliding_max"][4] == pytest.approx(9.0)
        assert sensor_states["sliding_median"][4] == pytest.approx(7.0)
        assert sensor_states["sliding_moving_avg"][4] == pytest.approx(7.0)
