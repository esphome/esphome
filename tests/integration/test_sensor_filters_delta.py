"""Test sensor DeltaFilter functionality."""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, EntityState, SensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_sensor_filters_delta(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    loop = asyncio.get_running_loop()

    sensor_values: dict[str, list[float]] = {
        "filter_min": [],
        "filter_max": [],
        "filter_baseline_max": [],
        "filter_zero_delta": [],
    }

    filter_min_done = loop.create_future()
    filter_max_done = loop.create_future()
    filter_baseline_max_done = loop.create_future()
    filter_zero_delta_done = loop.create_future()

    def on_state(state: EntityState) -> None:
        if not isinstance(state, SensorState) or state.missing_state:
            return

        sensor_name = key_to_sensor.get(state.key)
        if sensor_name not in sensor_values:
            return

        sensor_values[sensor_name].append(state.state)

        # Check completion conditions
        if (
            sensor_name == "filter_min"
            and len(sensor_values[sensor_name]) == 3
            and not filter_min_done.done()
        ):
            filter_min_done.set_result(True)
        elif (
            sensor_name == "filter_max"
            and len(sensor_values[sensor_name]) == 3
            and not filter_max_done.done()
        ):
            filter_max_done.set_result(True)
        elif (
            sensor_name == "filter_baseline_max"
            and len(sensor_values[sensor_name]) == 4
            and not filter_baseline_max_done.done()
        ):
            filter_baseline_max_done.set_result(True)
        elif (
            sensor_name == "filter_zero_delta"
            and len(sensor_values[sensor_name]) == 2
            and not filter_zero_delta_done.done()
        ):
            filter_zero_delta_done.set_result(True)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        # Get entities and build key mapping
        entities, _ = await client.list_entities_services()
        key_to_sensor = build_key_to_entity_mapping(
            entities,
            {
                "filter_min": "Filter Min",
                "filter_max": "Filter Max",
                "filter_baseline_max": "Filter Baseline Max",
                "filter_zero_delta": "Filter Zero Delta",
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
            "Test Filter Min": "filter_min",
            "Test Filter Max": "filter_max",
            "Test Filter Baseline Max": "filter_baseline_max",
            "Test Filter Zero Delta": "filter_zero_delta",
        }
        buttons = {}
        for entity in entities:
            if isinstance(entity, ButtonInfo) and entity.name in button_name_map:
                buttons[button_name_map[entity.name]] = entity.key

        assert len(buttons) == 4, f"Expected 3 buttons, found {len(buttons)}"

        # Test 1: Min
        sensor_values["filter_min"].clear()
        client.button_command(buttons["filter_min"])
        try:
            await asyncio.wait_for(filter_min_done, timeout=2.0)
        except TimeoutError:
            pytest.fail(f"Test 1 timed out. Values: {sensor_values['filter_min']}")

        expected = [1.0, 12.0, -2.0]
        assert sensor_values["filter_min"] == pytest.approx(expected), (
            f"Test 1 failed: expected {expected}, got {sensor_values['filter_min']}"
        )

        # Test 2: Max
        sensor_values["filter_max"].clear()
        client.button_command(buttons["filter_max"])
        try:
            await asyncio.wait_for(filter_max_done, timeout=2.0)
        except TimeoutError:
            pytest.fail(f"Test 2 timed out. Values: {sensor_values['filter_max']}")

        expected = [1.0, 5.0, 10.0]
        assert sensor_values["filter_max"] == pytest.approx(expected), (
            f"Test 2 failed: expected {expected}, got {sensor_values['filter_max']}"
        )

        # Test 3: Baseline Max
        sensor_values["filter_baseline_max"].clear()
        client.button_command(buttons["filter_baseline_max"])
        try:
            await asyncio.wait_for(filter_baseline_max_done, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Test 3 timed out. Values: {sensor_values['filter_baseline_max']}"
            )

        expected = [1.0, 2.0, 3.0, 20.0]
        assert sensor_values["filter_baseline_max"] == pytest.approx(expected), (
            f"Test 3 failed: expected {expected}, got {sensor_values['filter_baseline_max']}"
        )

        # Test 4: Zero Delta
        sensor_values["filter_zero_delta"].clear()
        client.button_command(buttons["filter_zero_delta"])
        try:
            await asyncio.wait_for(filter_zero_delta_done, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Test 4 timed out. Values: {sensor_values['filter_zero_delta']}"
            )

        expected = [1.0, 2.0]
        assert sensor_values["filter_zero_delta"] == pytest.approx(expected), (
            f"Test 4 failed: expected {expected}, got {sensor_values['filter_zero_delta']}"
        )
