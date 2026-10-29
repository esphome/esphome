"""Test sensor DeltaFilter functionality."""

from __future__ import annotations

import asyncio
import math

from aioesphomeapi import ButtonInfo, EntityState, SensorState
import pytest

from .state_utils import build_key_to_entity_mapping, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


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
        "filter_percentage": [],
        "filter_nan": [],
        "filter_min_max": [],
        "filter_percentage_max": [],
        "filter_inf_reference": [],
    }

    filter_min_done = loop.create_future()
    filter_max_done = loop.create_future()
    filter_baseline_max_done = loop.create_future()
    filter_zero_delta_done = loop.create_future()
    filter_percentage_done = loop.create_future()
    filter_nan_done = loop.create_future()
    filter_min_max_done = loop.create_future()
    filter_percentage_max_done = loop.create_future()
    inf_done = loop.create_future()

    def on_state(state: EntityState) -> None:
        if not isinstance(state, SensorState):
            return

        sensor_name = key_to_sensor.get(state.key)
        if sensor_name == "inf_done" and not inf_done.done():
            inf_done.set_result(True)
        if sensor_name not in sensor_values:
            return

        if state.missing_state:
            # Only the NaN test is interested in unavailable states
            if sensor_name != "filter_nan":
                return
            sensor_values[sensor_name].append(math.nan)
        else:
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
        elif (
            sensor_name == "filter_percentage"
            and len(sensor_values[sensor_name]) == 3
            and not filter_percentage_done.done()
        ):
            filter_percentage_done.set_result(True)
        elif (
            sensor_name == "filter_nan"
            and len(sensor_values[sensor_name]) == 3
            and not filter_nan_done.done()
        ):
            filter_nan_done.set_result(True)
        elif (
            sensor_name == "filter_min_max"
            and len(sensor_values[sensor_name]) == 3
            and not filter_min_max_done.done()
        ):
            filter_min_max_done.set_result(True)
        elif (
            sensor_name == "filter_percentage_max"
            and len(sensor_values[sensor_name]) == 3
            and not filter_percentage_max_done.done()
        ):
            filter_percentage_max_done.set_result(True)

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
                "filter_percentage": "Filter Percentage",
                "filter_nan": "Filter NaN",
                "filter_min_max": "Filter Min Max",
                "filter_percentage_max": "Filter Percentage Max",
                "filter_inf_reference": "Filter Inf Reference",
                "inf_done": "Inf Done",
            },
        )

        # Wait for initial states
        await subscribe_after_initial_states(client, entities, on_state=on_state)

        # Find all buttons
        button_name_map = {
            "Test Filter Min": "filter_min",
            "Test Filter Max": "filter_max",
            "Test Filter Baseline Max": "filter_baseline_max",
            "Test Filter Zero Delta": "filter_zero_delta",
            "Test Filter Percentage": "filter_percentage",
            "Test Filter NaN": "filter_nan",
            "Test Filter Min Max": "filter_min_max",
            "Test Filter Percentage Max": "filter_percentage_max",
            "Test Filter Inf Reference": "filter_inf_reference",
        }
        buttons = {}
        for entity in entities:
            if isinstance(entity, ButtonInfo) and entity.name in button_name_map:
                buttons[button_name_map[entity.name]] = entity.key

        assert len(buttons) == 9, f"Expected 9 buttons, found {len(buttons)}"

        # Test 1: Min
        sensor_values["filter_min"].clear()
        client.button_command(buttons["filter_min"])
        await wait_or_fail(
            filter_min_done, 2.0, "Test 1 timed out", values=sensor_values["filter_min"]
        )

        expected = [1.0, 12.0, -2.0]
        assert sensor_values["filter_min"] == pytest.approx(expected), (
            f"Test 1 failed: expected {expected}, got {sensor_values['filter_min']}"
        )

        # Test 2: Max
        sensor_values["filter_max"].clear()
        client.button_command(buttons["filter_max"])
        await wait_or_fail(
            filter_max_done, 2.0, "Test 2 timed out", values=sensor_values["filter_max"]
        )

        expected = [1.0, 5.0, 10.0]
        assert sensor_values["filter_max"] == pytest.approx(expected), (
            f"Test 2 failed: expected {expected}, got {sensor_values['filter_max']}"
        )

        # Test 3: Baseline Max
        sensor_values["filter_baseline_max"].clear()
        client.button_command(buttons["filter_baseline_max"])
        await wait_or_fail(
            filter_baseline_max_done,
            2.0,
            "Test 3 timed out",
            values=sensor_values["filter_baseline_max"],
        )

        expected = [1.0, 2.0, 3.0, 20.0]
        assert sensor_values["filter_baseline_max"] == pytest.approx(expected), (
            f"Test 3 failed: expected {expected}, got {sensor_values['filter_baseline_max']}"
        )

        # Test 4: Zero Delta
        sensor_values["filter_zero_delta"].clear()
        client.button_command(buttons["filter_zero_delta"])
        await wait_or_fail(
            filter_zero_delta_done,
            2.0,
            "Test 4 timed out",
            values=sensor_values["filter_zero_delta"],
        )

        expected = [1.0, 2.0]
        assert sensor_values["filter_zero_delta"] == pytest.approx(expected), (
            f"Test 4 failed: expected {expected}, got {sensor_values['filter_zero_delta']}"
        )

        # Test 5: Percentage (delta: 50%)
        sensor_values["filter_percentage"].clear()
        client.button_command(buttons["filter_percentage"])
        await wait_or_fail(
            filter_percentage_done,
            2.0,
            "Test 5 timed out",
            values=sensor_values["filter_percentage"],
        )

        expected = [100.0, 160.0, 250.0]
        assert sensor_values["filter_percentage"] == pytest.approx(expected), (
            f"Test 5 failed: expected {expected}, got {sensor_values['filter_percentage']}"
        )

        # Test 6: NaN passes through once, then is suppressed
        sensor_values["filter_nan"].clear()
        client.button_command(buttons["filter_nan"])
        await wait_or_fail(
            filter_nan_done, 2.0, "Test 6 timed out", values=sensor_values["filter_nan"]
        )

        values = sensor_values["filter_nan"]
        assert values[0] == pytest.approx(1.0), f"Test 6 failed: got {values}"
        assert math.isnan(values[1]), (
            f"Test 6 failed: NaN not passed through, got {values}"
        )
        assert values[2] == pytest.approx(2.0), f"Test 6 failed: got {values}"

        # Test 7: Min and max together; min is exclusive, max inclusive
        sensor_values["filter_min_max"].clear()
        client.button_command(buttons["filter_min_max"])
        await wait_or_fail(
            filter_min_max_done,
            2.0,
            "Test 7 timed out",
            values=sensor_values["filter_min_max"],
        )

        expected = [0.0, 10.0, 15.0]
        assert sensor_values["filter_min_max"] == pytest.approx(expected), (
            f"Test 7 failed: expected {expected}, got {sensor_values['filter_min_max']}"
        )

        # Test 8: Percentage max (max_value: 50%)
        sensor_values["filter_percentage_max"].clear()
        client.button_command(buttons["filter_percentage_max"])
        await wait_or_fail(
            filter_percentage_max_done,
            2.0,
            "Test 8 timed out",
            values=sensor_values["filter_percentage_max"],
        )

        expected = [100.0, 150.0, 75.0]
        assert sensor_values["filter_percentage_max"] == pytest.approx(expected), (
            f"Test 8 failed: expected {expected}, got {sensor_values['filter_percentage_max']}"
        )

        # Test 9: after an infinite value, absolute min + percentage max drops everything
        sensor_values["filter_inf_reference"].clear()
        client.button_command(buttons["filter_inf_reference"])
        await wait_or_fail(inf_done, 2.0, "Test 9 timed out")

        values = sensor_values["filter_inf_reference"]
        assert len(values) == 1 and math.isinf(values[0]), (
            f"Test 9 failed: got {values}"
        )
