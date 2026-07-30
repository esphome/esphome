"""Test sensor timeout filter functionality."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, SensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_sensor_timeout_filter(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test TimeoutFilter and TimeoutFilterConfigured with all modes."""
    loop = asyncio.get_running_loop()

    # Track state changes for all sensors
    timeout_last_states: list[float] = []
    timeout_reset_states: list[float] = []
    timeout_static_states: list[float] = []
    timeout_lambda_states: list[float] = []

    # Futures for each test scenario
    test1_complete = loop.create_future()  # TimeoutFilter - last mode
    test2_complete = loop.create_future()  # TimeoutFilter - reset behavior
    test3_complete = loop.create_future()  # TimeoutFilterConfigured - static value
    test4_complete = loop.create_future()  # TimeoutFilterConfigured - lambda

    def on_state(state: EntityState) -> None:
        """Track sensor state updates."""
        if not isinstance(state, SensorState):
            return

        if state.missing_state:
            return

        sensor_name = key_to_sensor.get(state.key)

        # Test 1: TimeoutFilter - last mode
        if sensor_name == "timeout_last_sensor":
            timeout_last_states.append(state.state)
            # Expect 2 values: initial 42.0 + timeout fires with 42.0
            if len(timeout_last_states) >= 2 and not test1_complete.done():
                test1_complete.set_result(True)

        # Test 2: TimeoutFilter - reset behavior
        elif sensor_name == "timeout_reset_sensor":
            timeout_reset_states.append(state.state)
            # Expect 4 values: 10.0, 20.0, 30.0, then timeout fires with 30.0
            if len(timeout_reset_states) >= 4 and not test2_complete.done():
                test2_complete.set_result(True)

        # Test 3: TimeoutFilterConfigured - static value
        elif sensor_name == "timeout_static_sensor":
            timeout_static_states.append(state.state)
            # Expect 2 values: initial 55.5 + timeout fires with 99.9
            if len(timeout_static_states) >= 2 and not test3_complete.done():
                test3_complete.set_result(True)

        # Test 4: TimeoutFilterConfigured - lambda
        elif sensor_name == "timeout_lambda_sensor":
            timeout_lambda_states.append(state.state)
            # Expect 2 values: initial 77.7 + timeout fires with -1.0
            if len(timeout_lambda_states) >= 2 and not test4_complete.done():
                test4_complete.set_result(True)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, services = await client.list_entities_services()

        key_to_sensor = build_key_to_entity_mapping(
            entities,
            [
                "timeout_last_sensor",
                "timeout_reset_sensor",
                "timeout_static_sensor",
                "timeout_lambda_sensor",
            ],
        )

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Helper to find buttons by object_id substring
        def find_button(object_id_substring: str) -> int:
            """Find a button by object_id substring and return its key."""
            button = next(
                (e for e in entities if object_id_substring in e.object_id.lower()),
                None,
            )
            assert button is not None, f"Button '{object_id_substring}' not found"
            return button.key

        # Find all test buttons
        test1_button_key = find_button("test_timeout_last_button")
        test2_button_key = find_button("test_timeout_reset_button")
        test3_button_key = find_button("test_timeout_static_button")
        test4_button_key = find_button("test_timeout_lambda_button")

        # === Test 1: TimeoutFilter - last mode ===
        client.button_command(test1_button_key)
        try:
            await asyncio.wait_for(test1_complete, timeout=2.0)
        except TimeoutError:
            pytest.fail(f"Test 1 timeout. Received states: {timeout_last_states}")

        assert len(timeout_last_states) == 2, (
            f"Test 1: Should have 2 states, got {len(timeout_last_states)}: {timeout_last_states}"
        )
        assert timeout_last_states[0] == pytest.approx(42.0), (
            f"Test 1: First state should be 42.0, got {timeout_last_states[0]}"
        )
        assert timeout_last_states[1] == pytest.approx(42.0), (
            f"Test 1: Timeout should output last value (42.0), got {timeout_last_states[1]}"
        )

        # === Test 2: TimeoutFilter - reset behavior ===
        client.button_command(test2_button_key)
        try:
            await asyncio.wait_for(test2_complete, timeout=2.0)
        except TimeoutError:
            pytest.fail(f"Test 2 timeout. Received states: {timeout_reset_states}")

        assert len(timeout_reset_states) == 4, (
            f"Test 2: Should have 4 states, got {len(timeout_reset_states)}: {timeout_reset_states}"
        )
        assert timeout_reset_states[0] == pytest.approx(10.0), (
            f"Test 2: First state should be 10.0, got {timeout_reset_states[0]}"
        )
        assert timeout_reset_states[1] == pytest.approx(20.0), (
            f"Test 2: Second state should be 20.0, got {timeout_reset_states[1]}"
        )
        assert timeout_reset_states[2] == pytest.approx(30.0), (
            f"Test 2: Third state should be 30.0, got {timeout_reset_states[2]}"
        )
        assert timeout_reset_states[3] == pytest.approx(30.0), (
            f"Test 2: Timeout should output last value (30.0), got {timeout_reset_states[3]}"
        )

        # === Test 3: TimeoutFilterConfigured - static value ===
        client.button_command(test3_button_key)
        try:
            await asyncio.wait_for(test3_complete, timeout=2.0)
        except TimeoutError:
            pytest.fail(f"Test 3 timeout. Received states: {timeout_static_states}")

        assert len(timeout_static_states) == 2, (
            f"Test 3: Should have 2 states, got {len(timeout_static_states)}: {timeout_static_states}"
        )
        assert timeout_static_states[0] == pytest.approx(55.5), (
            f"Test 3: First state should be 55.5, got {timeout_static_states[0]}"
        )
        assert timeout_static_states[1] == pytest.approx(99.9), (
            f"Test 3: Timeout should output configured value (99.9), got {timeout_static_states[1]}"
        )

        # === Test 4: TimeoutFilterConfigured - lambda ===
        client.button_command(test4_button_key)
        try:
            await asyncio.wait_for(test4_complete, timeout=2.0)
        except TimeoutError:
            pytest.fail(f"Test 4 timeout. Received states: {timeout_lambda_states}")

        assert len(timeout_lambda_states) == 2, (
            f"Test 4: Should have 2 states, got {len(timeout_lambda_states)}: {timeout_lambda_states}"
        )
        assert timeout_lambda_states[0] == pytest.approx(77.7), (
            f"Test 4: First state should be 77.7, got {timeout_lambda_states[0]}"
        )
        assert timeout_lambda_states[1] == pytest.approx(-1.0), (
            f"Test 4: Timeout should evaluate lambda (-1.0), got {timeout_lambda_states[1]}"
        )
