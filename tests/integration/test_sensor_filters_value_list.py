"""Test sensor ValueListFilter functionality (FilterOutValueFilter and ThrottleWithPriorityFilter)."""

from __future__ import annotations

import asyncio
import math

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
        "filter_out_nan_test": [],
        "filter_out_accuracy_2": [],
        "throttle_priority_nan": [],
    }

    # Futures for each test
    filter_out_single_done = loop.create_future()
    filter_out_multiple_done = loop.create_future()
    throttle_single_done = loop.create_future()
    throttle_multiple_done = loop.create_future()
    filter_out_nan_done = loop.create_future()
    filter_out_accuracy_2_done = loop.create_future()
    throttle_nan_done = loop.create_future()

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
        elif (
            sensor_name == "filter_out_nan_test"
            and len(sensor_values[sensor_name]) == 3
            and not filter_out_nan_done.done()
        ):
            filter_out_nan_done.set_result(True)
        elif (
            sensor_name == "filter_out_accuracy_2"
            and len(sensor_values[sensor_name]) == 2
            and not filter_out_accuracy_2_done.done()
        ):
            filter_out_accuracy_2_done.set_result(True)
        elif (
            sensor_name == "throttle_priority_nan"
            and len(sensor_values[sensor_name]) == 3
            and not throttle_nan_done.done()
        ):
            throttle_nan_done.set_result(True)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        # Get entities and build key mapping
        entities, _ = await client.list_entities_services()
        key_to_sensor = build_key_to_entity_mapping(
            entities,
            {
                "filter_out_single": "Filter Out Single",
                "filter_out_multiple": "Filter Out Multiple",
                "throttle_priority_single": "Throttle Priority Single",
                "throttle_priority_multiple": "Throttle Priority Multiple",
                "filter_out_nan_test": "Filter Out NaN Test",
                "filter_out_accuracy_2": "Filter Out Accuracy 2",
                "throttle_priority_nan": "Throttle Priority NaN",
            },
        )

        # Set up initial state helper with all entities
        initial_state_helper = InitialStateHelper(entities)

        # Subscribe to state changes with wrapper
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for initial states
        await initial_state_helper.wait_for_initial_states()

        # Find all buttons
        button_name_map = {
            "Test Filter Out Single": "filter_out_single",
            "Test Filter Out Multiple": "filter_out_multiple",
            "Test Throttle Priority Single": "throttle_priority_single",
            "Test Throttle Priority Multiple": "throttle_priority_multiple",
            "Test Filter Out NaN": "filter_out_nan",
            "Test Filter Out Accuracy 2": "filter_out_accuracy_2",
            "Test Throttle Priority NaN": "throttle_priority_nan",
        }
        buttons = {}
        for entity in entities:
            if isinstance(entity, ButtonInfo) and entity.name in button_name_map:
                buttons[button_name_map[entity.name]] = entity.key

        assert len(buttons) == 7, f"Expected 7 buttons, found {len(buttons)}"

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

        # Test 5: FilterOutValueFilter - NaN handling
        sensor_values["filter_out_nan_test"].clear()
        filter_out_nan_done = loop.create_future()
        client.button_command(buttons["filter_out_nan"])
        try:
            await asyncio.wait_for(filter_out_nan_done, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Test 5 timed out. Values: {sensor_values['filter_out_nan_test']}"
            )

        expected = [1.0, 2.0, 3.0]
        assert sensor_values["filter_out_nan_test"] == pytest.approx(expected), (
            f"Test 5 failed: expected {expected}, got {sensor_values['filter_out_nan_test']}"
        )

        # Test 6: FilterOutValueFilter - Accuracy decimals (2)
        sensor_values["filter_out_accuracy_2"].clear()
        filter_out_accuracy_2_done = loop.create_future()
        client.button_command(buttons["filter_out_accuracy_2"])
        try:
            await asyncio.wait_for(filter_out_accuracy_2_done, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Test 6 timed out. Values: {sensor_values['filter_out_accuracy_2']}"
            )

        expected = [42.01, 42.15]
        assert sensor_values["filter_out_accuracy_2"] == pytest.approx(expected), (
            f"Test 6 failed: expected {expected}, got {sensor_values['filter_out_accuracy_2']}"
        )

        # Test 7: ThrottleWithPriorityFilter - NaN priority
        sensor_values["throttle_priority_nan"].clear()
        throttle_nan_done = loop.create_future()
        client.button_command(buttons["throttle_priority_nan"])
        try:
            await asyncio.wait_for(throttle_nan_done, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Test 7 timed out. Values: {sensor_values['throttle_priority_nan']}"
            )

        # First value (1.0) + two NaN priority values
        # NaN values will be compared using math.isnan
        assert len(sensor_values["throttle_priority_nan"]) == 3, (
            f"Test 7 failed: expected 3 values, got {len(sensor_values['throttle_priority_nan'])}"
        )
        assert sensor_values["throttle_priority_nan"][0] == pytest.approx(1.0), (
            f"Test 7 failed: first value should be 1.0, got {sensor_values['throttle_priority_nan'][0]}"
        )
        assert math.isnan(sensor_values["throttle_priority_nan"][1]), (
            f"Test 7 failed: second value should be NaN, got {sensor_values['throttle_priority_nan'][1]}"
        )
        assert math.isnan(sensor_values["throttle_priority_nan"][2]), (
            f"Test 7 failed: third value should be NaN, got {sensor_values['throttle_priority_nan'][2]}"
        )
