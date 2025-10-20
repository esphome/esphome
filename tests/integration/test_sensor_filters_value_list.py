"""Test sensor ValueListFilter functionality (FilterOutValueFilter and ThrottleWithPriorityFilter)."""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, EntityState, SensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_sensor_filters_value_list(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that ValueListFilter-based filters work correctly."""
    loop = asyncio.get_running_loop()

    # Track state changes for all sensors
    sensor_values: dict[str, list[float]] = {
        "filter_out_single": [],
        "filter_out_multiple": [],
        "throttle_priority_single": [],
        "throttle_priority_multiple": [],
    }

    # Futures for each test
    filter_out_single_done = loop.create_future()
    filter_out_multiple_done = loop.create_future()
    throttle_single_done = loop.create_future()
    throttle_multiple_done = loop.create_future()

    def on_state(state: EntityState) -> None:
        """Track sensor state updates."""
        if not isinstance(state, SensorState) or state.missing_state:
            return

        sensor_name = key_to_sensor.get(state.key)
        if sensor_name not in sensor_values:
            return

        sensor_values[sensor_name].append(state.state)

        # Check completion conditions
        if (
            sensor_name == "filter_out_single"
            and len(sensor_values[sensor_name]) == 3
            and not filter_out_single_done.done()
        ):
            filter_out_single_done.set_result(True)
        elif (
            sensor_name == "filter_out_multiple"
            and len(sensor_values[sensor_name]) == 3
            and not filter_out_multiple_done.done()
        ):
            filter_out_multiple_done.set_result(True)
        elif (
            sensor_name == "throttle_priority_single"
            and len(sensor_values[sensor_name]) == 3
            and not throttle_single_done.done()
        ):
            throttle_single_done.set_result(True)
        elif (
            sensor_name == "throttle_priority_multiple"
            and len(sensor_values[sensor_name]) == 4
            and not throttle_multiple_done.done()
        ):
            throttle_multiple_done.set_result(True)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        # Get entities and build key mapping
        entities, services = await client.list_entities_services()
        key_to_sensor = build_key_to_entity_mapping(
            entities,
            {
                "filter_out_single": "Filter Out Single",
                "filter_out_multiple": "Filter Out Multiple",
                "throttle_priority_single": "Throttle Priority Single",
                "throttle_priority_multiple": "Throttle Priority Multiple",
            },
        )

        # Set up initial state helper with all entities
        initial_state_helper = InitialStateHelper(entities)

        # Subscribe to state changes with wrapper
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for initial states
        await initial_state_helper.wait_for_initial_states()

        # Find all buttons
        buttons = {}
        for entity in entities:
            if isinstance(entity, ButtonInfo):
                if entity.name == "Test Filter Out Single":
                    buttons["filter_out_single"] = entity.key
                elif entity.name == "Test Filter Out Multiple":
                    buttons["filter_out_multiple"] = entity.key
                elif entity.name == "Test Throttle Priority Single":
                    buttons["throttle_priority_single"] = entity.key
                elif entity.name == "Test Throttle Priority Multiple":
                    buttons["throttle_priority_multiple"] = entity.key

        assert len(buttons) == 4, f"Expected 4 buttons, found {len(buttons)}"

        # Test 1: FilterOutValueFilter - single value
        sensor_values["filter_out_single"].clear()
        client.button_command(buttons["filter_out_single"])
        try:
            await asyncio.wait_for(filter_out_single_done, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Test 1 timed out. Values: {sensor_values['filter_out_single']}"
            )

        expected = [1.0, 2.0, 3.0]
        assert sensor_values["filter_out_single"] == pytest.approx(expected), (
            f"Test 1 failed: expected {expected}, got {sensor_values['filter_out_single']}"
        )

        # Test 2: FilterOutValueFilter - multiple values
        sensor_values["filter_out_multiple"].clear()
        filter_out_multiple_done = loop.create_future()
        client.button_command(buttons["filter_out_multiple"])
        try:
            await asyncio.wait_for(filter_out_multiple_done, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Test 2 timed out. Values: {sensor_values['filter_out_multiple']}"
            )

        expected = [1.0, 2.0, 50.0]
        assert sensor_values["filter_out_multiple"] == pytest.approx(expected), (
            f"Test 2 failed: expected {expected}, got {sensor_values['filter_out_multiple']}"
        )

        # Test 3: ThrottleWithPriorityFilter - single priority
        sensor_values["throttle_priority_single"].clear()
        throttle_single_done = loop.create_future()
        client.button_command(buttons["throttle_priority_single"])
        try:
            await asyncio.wait_for(throttle_single_done, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Test 3 timed out. Values: {sensor_values['throttle_priority_single']}"
            )

        expected = [1.0, 42.0, 4.0]
        assert sensor_values["throttle_priority_single"] == pytest.approx(expected), (
            f"Test 3 failed: expected {expected}, got {sensor_values['throttle_priority_single']}"
        )

        # Test 4: ThrottleWithPriorityFilter - multiple priorities
        sensor_values["throttle_priority_multiple"].clear()
        throttle_multiple_done = loop.create_future()
        client.button_command(buttons["throttle_priority_multiple"])
        try:
            await asyncio.wait_for(throttle_multiple_done, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Test 4 timed out. Values: {sensor_values['throttle_priority_multiple']}"
            )

        expected = [1.0, 0.0, 42.0, 100.0]
        assert sensor_values["throttle_priority_multiple"] == pytest.approx(expected), (
            f"Test 4 failed: expected {expected}, got {sensor_values['throttle_priority_multiple']}"
        )
