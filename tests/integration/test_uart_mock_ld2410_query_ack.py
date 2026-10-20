"""Integration test for the LD2410 query parameters ACK with mock UART.

An ordinary data frame comes first, then two query ACKs. Move thresholds 4, 3, 2, 1
put the command footer inside the ACK values. Every value must come from the whole
38 byte frame, not from bytes left by the previous frame.
The ACKs are synthetic, built from the layout the component parses, not device captures.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    ButtonInfo,
    EntityState,
    NumberInfo,
    NumberState,
    SensorInfo,
    SensorState,
)
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping, find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

# Number object_id: (value from the seed ACK, value from the ACK under test)
EXPECTED: dict[str, tuple[float, float]] = {
    "max_move_distance_gate": (8.0, 6.0),
    "max_still_distance_gate": (8.0, 5.0),
    "timeout": (5.0, 300.0),
    "g0_move_threshold": (50.0, 4.0),
    "g0_still_threshold": (0.0, 20.0),
    "g4_move_threshold": (20.0, 60.0),
    "g4_still_threshold": (40.0, 24.0),
    "g8_move_threshold": (15.0, 48.0),
    "g8_still_threshold": (15.0, 28.0),
}

# Sensor object_id: value from the data frame
SENSOR_EXPECTED: dict[str, float] = {
    "moving_distance": 100.0,
    "still_distance": 120.0,
    "moving_energy": 50.0,
    "still_energy": 25.0,
    "detection_distance": 300.0,
}


@pytest.mark.asyncio
async def test_uart_mock_ld2410_query_ack(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test a data frame, then a query ACK with the footer bytes in its values."""
    loop = asyncio.get_running_loop()
    ack_errors: list[str] = []
    waited_for_frame_end = loop.create_future()

    def line_callback(line: str) -> None:
        if "[ld2410" not in line:
            return
        if "Invalid " in line:
            ack_errors.append(line)
        if "Ack Data incomplete" in line and not waited_for_frame_end.done():
            waited_for_frame_end.set_result(True)

    number_states: dict[str, list[float]] = {name: [] for name in EXPECTED}
    sensor_states: dict[str, list[float]] = {name: [] for name in SENSOR_EXPECTED}
    key_to_name: dict[int, str] = {}
    sensor_key_to_name: dict[int, str] = {}
    data_received = loop.create_future()
    final_received = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            if (name := sensor_key_to_name.get(state.key)) is None:
                return
            sensor_states[name].append(state.state)
            if not data_received.done() and all(sensor_states.values()):
                data_received.set_result(True)
            return
        if not isinstance(state, NumberState) or state.missing_state:
            return
        if (name := key_to_name.get(state.key)) is None:
            return
        number_states[name].append(state.state)
        if not final_received.done() and all(
            values and values[-1] == pytest.approx(EXPECTED[n][1])
            for n, values in number_states.items()
        ):
            final_received.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        # Map each kind separately; sensor names are substrings of number names
        key_to_name.update(
            build_key_to_entity_mapping(
                [e for e in entities if isinstance(e, NumberInfo)], list(EXPECTED)
            )
        )
        sensor_key_to_name.update(
            build_key_to_entity_mapping(
                [e for e in entities if isinstance(e, SensorInfo)],
                list(SENSOR_EXPECTED),
            )
        )
        assert set(key_to_name.values()) == set(EXPECTED), (
            f"Number entities missing, found: {sorted(key_to_name.values())}"
        )
        assert set(sensor_key_to_name.values()) == set(SENSOR_EXPECTED), (
            f"Sensor entities missing, found: {sorted(sensor_key_to_name.values())}"
        )

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        try:
            await asyncio.wait_for(data_received, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for the data frame values. Received:\n"
                f"  sensor_states: {sensor_states}"
            )

        try:
            await asyncio.wait_for(final_received, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for the query ACK values. Received:\n"
                f"  number_states: {number_states}\n"
                f"  ack_errors: {ack_errors}"
            )

        # One data frame, so each sensor is published once
        for name, value in SENSOR_EXPECTED.items():
            assert sensor_states[name] == pytest.approx([value]), name
        # Each number is published once per ACK: the seed value, then the value under test
        for name, (seed, value) in EXPECTED.items():
            assert number_states[name] == pytest.approx([seed, value]), name
        assert waited_for_frame_end.done(), (
            "Expected the parser to wait past the footer bytes in the values"
        )
        assert not ack_errors, f"Unexpected ACK errors: {ack_errors}"
