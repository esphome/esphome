"""Integration test for LD2420 component with mock UART.

Tests:
test_uart_mock_ld2420 (energy mode):
  1. Happy path - valid energy frame publishes correct sensor values
  2. Garbage resilience - random bytes don't crash the component
  3. Truncated energy frame - triggers "Energy frame too short" warning (PR #14458 bug #3)
  4. Buffer overflow recovery - overflow resets the parser
  5. Post-overflow parsing - next valid frame after overflow is parsed correctly
  6. TX logging - verifies LD2420 sends expected setup commands

test_uart_mock_ld2420_simple (simple mode):
  1. Happy path - valid simple mode text frame publishes correct values
  2. Garbage resilience
  3. Buffer overflow recovery
  4. 16-digit distance triggers infinite loop pre-fix (PR #14458 bug #1)
  5. Post-bug-trigger recovery proves the parser survived
"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import (
    BinarySensorInfo,
    BinarySensorState,
    EntityState,
    SensorInfo,
    SensorState,
)
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping, find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_ld2420(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2420 energy mode: happy path, truncated frame, overflow, and recovery."""
    # Replace external component path placeholder
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()

    # Track overflow warning in logs
    overflow_seen = loop.create_future()

    # Track "Energy frame too short" warning (PR #14458 bug #3 fix)
    # This message ONLY exists after the fix. Pre-fix, handle_energy_mode_
    # silently reads past the buffer without any warning.
    truncated_frame_warning_seen = loop.create_future()

    # Track TX data logged by the mock for assertions
    tx_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "Max command length exceeded" in line and not overflow_seen.done():
            overflow_seen.set_result(True)
        if "Energy frame too short" in line and not truncated_frame_warning_seen.done():
            truncated_frame_warning_seen.set_result(True)
        # Capture all TX log lines from uart_mock
        if "uart_mock" in line and "TX " in line:
            tx_log_lines.append(line)

    # Track sensor state updates (after initial state is swallowed)
    sensor_states: dict[str, list[float]] = {
        "moving_distance": [],
    }
    binary_states: dict[str, list[bool]] = {
        "has_target": [],
    }

    # Signal when we see recovery frame values
    recovery_received = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in sensor_states:
                sensor_states[sensor_name].append(state.state)
                # Check if this is the recovery frame (moving_distance = 50)
                if (
                    sensor_name == "moving_distance"
                    and state.state == pytest.approx(50.0)
                    and not recovery_received.done()
                ):
                    recovery_received.set_result(True)
        elif isinstance(state, BinarySensorState):
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in binary_states:
                binary_states[sensor_name].append(state.state)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        # Build key mappings for all sensor types
        all_names = list(sensor_states.keys()) + list(binary_states.keys())
        all_names.sort(key=len, reverse=True)
        key_to_sensor = build_key_to_entity_mapping(entities, all_names)

        # Set up initial state helper
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Phase 1 values are in the initial states (swallowed by InitialStateHelper).
        # Verify them via initial_states dict.
        moving_dist_entity = find_entity(entities, "moving_distance", SensorInfo)
        assert moving_dist_entity is not None
        initial_moving = initial_state_helper.initial_states.get(moving_dist_entity.key)
        assert initial_moving is not None and isinstance(initial_moving, SensorState)
        assert initial_moving.state == pytest.approx(100.0), (
            f"Initial moving distance should be 100, got {initial_moving.state}"
        )

        # Verify binary sensor detected target
        has_target_entity = find_entity(entities, "has_target", BinarySensorInfo)
        assert has_target_entity is not None
        initial_ht = initial_state_helper.initial_states.get(has_target_entity.key)
        assert initial_ht is not None and isinstance(initial_ht, BinarySensorState)
        assert initial_ht.state is True, "Has target should be True"

        # Wait for the recovery frame (Phase 5) to be parsed
        # This proves the component survived garbage + truncated + overflow
        try:
            await asyncio.wait_for(recovery_received, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for recovery frame. Received sensor states:\n"
                f"  moving_distance: {sensor_states['moving_distance']}"
            )

        # Verify overflow warning was logged
        assert overflow_seen.done(), (
            "Expected 'Max command length exceeded' warning in logs"
        )

        # Verify truncated frame warning was logged (PR #14458 bug #3)
        # This assertion FAILS before PR #14458 because the length check
        # and warning message did not exist.
        assert truncated_frame_warning_seen.done(), (
            "Expected 'Energy frame too short' warning in logs. "
            "This indicates PR #14458 fix for handle_energy_mode_ length "
            "validation is missing."
        )

        # Verify LD2420 sent setup commands (TX logging)
        assert len(tx_log_lines) > 0, "Expected TX log lines from uart_mock"
        tx_data = " ".join(tx_log_lines)
        # Verify command frame header appears (FD:FC:FB:FA)
        assert "FD.FC.FB.FA" in tx_data or "FD:FC:FB:FA" in tx_data, (
            "Expected LD2420 command frame header FD:FC:FB:FA in TX log"
        )
        # Verify command frame footer appears (04:03:02:01)
        assert "04.03.02.01" in tx_data or "04:03:02:01" in tx_data, (
            "Expected LD2420 command frame footer 04:03:02:01 in TX log"
        )

        # Recovery frame values (Phase 5, after overflow)
        assert len(sensor_states["moving_distance"]) >= 1, (
            f"Expected recovery moving_distance, got: {sensor_states['moving_distance']}"
        )
        recovery_values = [
            v for v in sensor_states["moving_distance"] if v == pytest.approx(50.0)
        ]
        assert len(recovery_values) >= 1, (
            f"Expected moving_distance=50 in recovery, got: {sensor_states['moving_distance']}"
        )


@pytest.mark.asyncio
async def test_uart_mock_ld2420_simple(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2420 simple mode: happy path, overflow, and 16-digit bug trigger."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()

    # Track overflow warning in logs
    overflow_seen = loop.create_future()

    def line_callback(line: str) -> None:
        if "Max command length exceeded" in line and not overflow_seen.done():
            overflow_seen.set_result(True)

    # Track sensor state updates
    sensor_states: dict[str, list[float]] = {
        "moving_distance": [],
    }
    binary_states: dict[str, list[bool]] = {
        "has_target": [],
    }

    # Signal for recovery frames
    recovery_received = loop.create_future()
    post_bug_received = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in sensor_states:
                sensor_states[sensor_name].append(state.state)
                if (
                    sensor_name == "moving_distance"
                    and state.state == pytest.approx(50.0)
                    and not recovery_received.done()
                ):
                    recovery_received.set_result(True)
                # Phase 6: distance=25 proves Phase 5 (16-digit bug) didn't hang
                if (
                    sensor_name == "moving_distance"
                    and state.state == pytest.approx(25.0)
                    and not post_bug_received.done()
                ):
                    post_bug_received.set_result(True)
        elif isinstance(state, BinarySensorState):
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in binary_states:
                binary_states[sensor_name].append(state.state)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        all_names = list(sensor_states.keys()) + list(binary_states.keys())
        all_names.sort(key=len, reverse=True)
        key_to_sensor = build_key_to_entity_mapping(entities, all_names)

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Phase 1: simple mode "ON Range 0100\r\n" → distance=100, presence=true
        moving_dist_entity = find_entity(entities, "moving_distance", SensorInfo)
        assert moving_dist_entity is not None
        initial_moving = initial_state_helper.initial_states.get(moving_dist_entity.key)
        assert initial_moving is not None and isinstance(initial_moving, SensorState)
        assert initial_moving.state == pytest.approx(100.0), (
            f"Initial moving distance should be 100, got {initial_moving.state}"
        )

        has_target_entity = find_entity(entities, "has_target", BinarySensorInfo)
        assert has_target_entity is not None
        initial_ht = initial_state_helper.initial_states.get(has_target_entity.key)
        assert initial_ht is not None and isinstance(initial_ht, BinarySensorState)
        assert initial_ht.state is True, "Has target should be True"

        # Wait for Phase 4 recovery (distance=50) after overflow
        try:
            await asyncio.wait_for(recovery_received, timeout=5.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for recovery frame. Received:\n"
                f"  moving_distance: {sensor_states['moving_distance']}"
            )

        # Verify overflow warning was logged
        assert overflow_seen.done(), (
            "Expected 'Max command length exceeded' warning in logs"
        )

        # Wait for Phase 6: distance=25 (post-16-digit-bug recovery)
        # This assertion FAILS before PR #14458 because the 16-digit frame
        # in Phase 5 causes an infinite loop in handle_simple_mode_ pre-fix.
        # The binary hangs, Phase 6 never fires, and this wait times out.
        try:
            await asyncio.wait_for(post_bug_received, timeout=8.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for post-bug recovery (distance=25). "
                f"This likely means Phase 5 (16-digit frame) caused an infinite "
                f"loop in handle_simple_mode_, indicating PR #14458 bug #1 fix "
                f"is missing.\n"
                f"  moving_distance values: {sensor_states['moving_distance']}"
            )

        # Verify post-bug value
        post_bug_values = [
            v for v in sensor_states["moving_distance"] if v == pytest.approx(25.0)
        ]
        assert len(post_bug_values) >= 1, (
            f"Expected moving_distance=25 after 16-digit test, "
            f"got: {sensor_states['moving_distance']}"
        )
