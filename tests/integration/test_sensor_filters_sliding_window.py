"""Test sensor sliding window filter functionality."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, SensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_sensor_filters_sliding_window(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that sliding window filters (min, max, median, quantile, moving_average) work correctly."""
    loop = asyncio.get_running_loop()

    # Track state changes for each sensor
    sensor_states: dict[str, list[float]] = {
        "min_sensor": [],
        "max_sensor": [],
        "median_sensor": [],
        "quantile_sensor": [],
        "moving_avg_sensor": [],
    }

    # Futures to track when we receive expected values
    min_received = loop.create_future()
    max_received = loop.create_future()
    median_received = loop.create_future()
    quantile_received = loop.create_future()
    moving_avg_received = loop.create_future()

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

        # Check if we received the expected final value
        # After publishing 10 values [1.0, 2.0, ..., 10.0], the window has the last 5: [2, 3, 4, 5, 6]
        # Filters send at position 1 and position 6 (send_every=5 means every 5th value after first)
        if (
            sensor_name == "min_sensor"
            and state.state == pytest.approx(2.0)
            and not min_received.done()
        ):
            min_received.set_result(True)
        elif (
            sensor_name == "max_sensor"
            and state.state == pytest.approx(6.0)
            and not max_received.done()
        ):
            max_received.set_result(True)
        elif (
            sensor_name == "median_sensor"
            and state.state == pytest.approx(4.0)
            and not median_received.done()
        ):
            # Median of [2, 3, 4, 5, 6] = 4
            median_received.set_result(True)
        elif (
            sensor_name == "quantile_sensor"
            and state.state == pytest.approx(6.0)
            and not quantile_received.done()
        ):
            # 90th percentile of [2, 3, 4, 5, 6] = 6
            quantile_received.set_result(True)
        elif (
            sensor_name == "moving_avg_sensor"
            and state.state == pytest.approx(4.0)
            and not moving_avg_received.done()
        ):
            # Average of [2, 3, 4, 5, 6] = 4
            moving_avg_received.set_result(True)

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
                "min_sensor",
                "max_sensor",
                "median_sensor",
                "quantile_sensor",
                "moving_avg_sensor",
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

        # Wait for all sensors to receive their final values
        try:
            await asyncio.wait_for(
                asyncio.gather(
                    min_received,
                    max_received,
                    median_received,
                    quantile_received,
                    moving_avg_received,
                ),
                timeout=10.0,
            )
        except TimeoutError:
            # Provide detailed failure info
            pytest.fail(
                f"Timeout waiting for expected values. Received states:\n"
                f"  min: {sensor_states['min_sensor']}\n"
                f"  max: {sensor_states['max_sensor']}\n"
                f"  median: {sensor_states['median_sensor']}\n"
                f"  quantile: {sensor_states['quantile_sensor']}\n"
                f"  moving_avg: {sensor_states['moving_avg_sensor']}"
            )

        # Verify we got the expected values
        # With batch_delay: 0ms, we should receive all outputs
        # Filters output at positions 1 and 6 (send_every: 5)
        assert len(sensor_states["min_sensor"]) == 2, (
            f"Min sensor should have 2 values, got {len(sensor_states['min_sensor'])}: {sensor_states['min_sensor']}"
        )
        assert len(sensor_states["max_sensor"]) == 2, (
            f"Max sensor should have 2 values, got {len(sensor_states['max_sensor'])}: {sensor_states['max_sensor']}"
        )
        assert len(sensor_states["median_sensor"]) == 2
        assert len(sensor_states["quantile_sensor"]) == 2
        assert len(sensor_states["moving_avg_sensor"]) == 2

        # Verify the first output (after 1 value: [1])
        assert sensor_states["min_sensor"][0] == pytest.approx(1.0), (
            f"First min should be 1.0, got {sensor_states['min_sensor'][0]}"
        )
        assert sensor_states["max_sensor"][0] == pytest.approx(1.0), (
            f"First max should be 1.0, got {sensor_states['max_sensor'][0]}"
        )
        assert sensor_states["median_sensor"][0] == pytest.approx(1.0), (
            f"First median should be 1.0, got {sensor_states['median_sensor'][0]}"
        )
        assert sensor_states["moving_avg_sensor"][0] == pytest.approx(1.0), (
            f"First moving avg should be 1.0, got {sensor_states['moving_avg_sensor'][0]}"
        )

        # Verify the second output (after 6 values, window has [2, 3, 4, 5, 6])
        assert sensor_states["min_sensor"][1] == pytest.approx(2.0), (
            f"Second min should be 2.0, got {sensor_states['min_sensor'][1]}"
        )
        assert sensor_states["max_sensor"][1] == pytest.approx(6.0), (
            f"Second max should be 6.0, got {sensor_states['max_sensor'][1]}"
        )
        assert sensor_states["median_sensor"][1] == pytest.approx(4.0), (
            f"Second median should be 4.0, got {sensor_states['median_sensor'][1]}"
        )
        assert sensor_states["moving_avg_sensor"][1] == pytest.approx(4.0), (
            f"Second moving avg should be 4.0, got {sensor_states['moving_avg_sensor'][1]}"
        )


@pytest.mark.asyncio
async def test_sensor_filters_nan_handling(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that sliding window filters handle NaN values correctly."""
    loop = asyncio.get_running_loop()

    # Track states
    min_states: list[float] = []
    max_states: list[float] = []

    # Future to track completion
    filters_completed = loop.create_future()

    def on_state(state: EntityState) -> None:
        """Track sensor state updates."""
        if not isinstance(state, SensorState):
            return

        # Skip NaN values
        if state.missing_state:
            return

        sensor_name = key_to_sensor.get(state.key)

        if sensor_name == "min_nan":
            min_states.append(state.state)
        elif sensor_name == "max_nan":
            max_states.append(state.state)

        # Check if both have received their final values
        # With batch_delay: 0ms, we should receive 2 outputs each
        if (
            len(min_states) >= 2
            and len(max_states) >= 2
            and not filters_completed.done()
        ):
            filters_completed.set_result(True)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        # Get entities first to build key mapping
        entities, services = await client.list_entities_services()

        # Build key-to-sensor mapping
        key_to_sensor = build_key_to_entity_mapping(entities, ["min_nan", "max_nan"])

        # Set up initial state helper with all entities
        initial_state_helper = InitialStateHelper(entities)

        # Subscribe to state changes with wrapper
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for initial states
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Find the publish button
        publish_button = next(
            (e for e in entities if "publish_nan_values_button" in e.object_id.lower()),
            None,
        )
        assert publish_button is not None, "Publish NaN Values Button not found"

        # Press the button
        client.button_command(publish_button.key)

        # Wait for filters to process
        try:
            await asyncio.wait_for(filters_completed, timeout=10.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for NaN handling. Received:\n"
                f"  min_states: {min_states}\n"
                f"  max_states: {max_states}"
            )

        # Verify NaN values were ignored
        # With batch_delay: 0ms, we should receive both outputs (at positions 1 and 6)
        # Position 1: window=[10], min=10, max=10
        # Position 6: window=[NaN, 5, NaN, 15, 8], ignoring NaN -> [5, 15, 8], min=5, max=15
        assert len(min_states) == 2, (
            f"Should have 2 min states, got {len(min_states)}: {min_states}"
        )
        assert len(max_states) == 2, (
            f"Should have 2 max states, got {len(max_states)}: {max_states}"
        )

        # First output
        assert min_states[0] == pytest.approx(10.0), (
            f"First min should be 10.0, got {min_states[0]}"
        )
        assert max_states[0] == pytest.approx(10.0), (
            f"First max should be 10.0, got {max_states[0]}"
        )

        # Second output - verify NaN values were ignored
        assert min_states[1] == pytest.approx(5.0), (
            f"Second min should ignore NaN and return 5.0, got {min_states[1]}"
        )
        assert max_states[1] == pytest.approx(15.0), (
            f"Second max should ignore NaN and return 15.0, got {max_states[1]}"
        )


@pytest.mark.asyncio
async def test_sensor_filters_ring_buffer_wraparound(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that ring buffer correctly wraps around when window fills up."""
    loop = asyncio.get_running_loop()

    min_states: list[float] = []

    test_completed = loop.create_future()

    def on_state(state: EntityState) -> None:
        """Track min sensor states."""
        if not isinstance(state, SensorState):
            return

        # Skip NaN values
        if state.missing_state:
            return

        sensor_name = key_to_sensor.get(state.key)

        if sensor_name == "wraparound_min":
            min_states.append(state.state)
            # With batch_delay: 0ms, we should receive all 3 outputs
            if len(min_states) >= 3 and not test_completed.done():
                test_completed.set_result(True)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        # Get entities first to build key mapping
        entities, services = await client.list_entities_services()

        # Build key-to-sensor mapping
        key_to_sensor = build_key_to_entity_mapping(entities, ["wraparound_min"])

        # Set up initial state helper with all entities
        initial_state_helper = InitialStateHelper(entities)

        # Subscribe to state changes with wrapper
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for initial state
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial state")

        # Find the publish button
        publish_button = next(
            (e for e in entities if "publish_wraparound_button" in e.object_id.lower()),
            None,
        )
        assert publish_button is not None, "Publish Wraparound Button not found"

        # Press the button
        # Will publish: 10, 20, 30, 5, 25, 15, 40, 35, 20
        client.button_command(publish_button.key)

        # Wait for completion
        try:
            await asyncio.wait_for(test_completed, timeout=10.0)
        except TimeoutError:
            pytest.fail(f"Timeout waiting for wraparound test. Received: {min_states}")

        # Verify outputs
        # With window_size=3, send_every=3, we get outputs at positions 1, 4, 7
        # Position 1: window=[10], min=10
        # Position 4: window=[20, 30, 5], min=5
        # Position 7: window=[15, 40, 35], min=15
        # With batch_delay: 0ms, we should receive all 3 outputs
        assert len(min_states) == 3, (
            f"Should have 3 states, got {len(min_states)}: {min_states}"
        )
        assert min_states[0] == pytest.approx(10.0), (
            f"First min should be 10.0, got {min_states[0]}"
        )
        assert min_states[1] == pytest.approx(5.0), (
            f"Second min should be 5.0, got {min_states[1]}"
        )
        assert min_states[2] == pytest.approx(15.0), (
            f"Third min should be 15.0, got {min_states[2]}"
        )
