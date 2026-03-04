"""Integration test for LD2412 component with mock UART.

Tests:
test_uart_mock_ld2412 (normal mode):
  1. Happy path - valid data frame publishes correct sensor values
  2. Garbage resilience - random bytes don't crash the component
  3. Truncated frame handling - partial frame doesn't corrupt state
  4. Buffer overflow recovery - overflow resets the parser
  5. Post-overflow parsing - next valid frame after overflow is parsed correctly
  6. TX logging - verifies LD2412 sends expected setup commands

test_uart_mock_ld2412_engineering (engineering mode):
  1. Engineering mode frames with per-gate energy data and light sensor
  2. Multi-byte still distance (291cm) using high byte > 0
  3. Gate energy sensor values
  4. Detection distance computed from target state
"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import BinarySensorState, ButtonInfo, EntityState, SensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping, find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_ld2412(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2412 data parsing with happy path, garbage, overflow, and recovery."""
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

    # Track TX data logged by the mock for assertions
    tx_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "Max command length exceeded" in line and not overflow_seen.done():
            overflow_seen.set_result(True)
        # Capture all TX log lines from uart_mock
        if "uart_mock" in line and "TX " in line:
            tx_log_lines.append(line)

    # Track sensor state updates (after initial state is swallowed)
    sensor_states: dict[str, list[float]] = {
        "moving_distance": [],
        "still_distance": [],
        "moving_energy": [],
        "still_energy": [],
        "detection_distance": [],
    }
    binary_states: dict[str, list[bool]] = {
        "has_target": [],
        "has_moving_target": [],
        "has_still_target": [],
    }

    # Signal when we see Phase 1 frame values (moving_distance = 100)
    phase1_received = loop.create_future()
    # Signal when we see recovery frame values
    recovery_received = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in sensor_states:
                sensor_states[sensor_name].append(state.state)
                if (
                    sensor_name == "moving_distance"
                    and state.state == pytest.approx(100.0)
                    and not phase1_received.done()
                ):
                    phase1_received.set_result(True)
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
        # Sort by descending length to avoid substring collisions
        # (e.g., "still_energy" matching "gate_0_still_energy")
        all_names.sort(key=len, reverse=True)
        key_to_sensor = build_key_to_entity_mapping(entities, all_names)

        # Set up initial state helper
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

        # Wait for Phase 1 frame to be received
        try:
            await asyncio.wait_for(phase1_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 1 frame. Received:\n"
                f"  moving_distance: {sensor_states['moving_distance']}"
            )

        # Phase 1 values: moving=100, still=120, energy=50/25, detect=100
        assert sensor_states["moving_distance"][0] == pytest.approx(100.0), (
            f"Phase 1 moving distance should be 100, got {sensor_states['moving_distance'][0]}"
        )
        assert sensor_states["still_distance"][0] == pytest.approx(120.0), (
            f"Phase 1 still distance should be 120, got {sensor_states['still_distance'][0]}"
        )
        assert sensor_states["moving_energy"][0] == pytest.approx(50.0), (
            f"Phase 1 moving energy should be 50, got {sensor_states['moving_energy'][0]}"
        )
        assert sensor_states["still_energy"][0] == pytest.approx(25.0), (
            f"Phase 1 still energy should be 25, got {sensor_states['still_energy'][0]}"
        )
        # LD2412 detection_distance = moving_distance when MOVE_BITMASK is set
        assert sensor_states["detection_distance"][0] == pytest.approx(100.0), (
            f"Phase 1 detection distance should be 100, got {sensor_states['detection_distance'][0]}"
        )

        # Wait for the recovery frame (Phase 5) to be parsed
        # This proves the component survived garbage + truncated + overflow
        try:
            await asyncio.wait_for(recovery_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for recovery frame. Received sensor states:\n"
                f"  moving_distance: {sensor_states['moving_distance']}\n"
                f"  still_distance: {sensor_states['still_distance']}\n"
                f"  moving_energy: {sensor_states['moving_energy']}\n"
                f"  still_energy: {sensor_states['still_energy']}\n"
                f"  detection_distance: {sensor_states['detection_distance']}"
            )

        # Verify overflow warning was logged
        assert overflow_seen.done(), (
            "Expected 'Max command length exceeded' warning in logs"
        )

        # Verify LD2412 sent setup commands (TX logging)
        assert len(tx_log_lines) > 0, "Expected TX log lines from uart_mock"
        tx_data = " ".join(tx_log_lines)
        # Verify command frame header appears (FD:FC:FB:FA)
        assert "FD:FC:FB:FA" in tx_data, (
            "Expected LD2412 command frame header FD:FC:FB:FA in TX log"
        )
        # Verify command frame footer appears (04:03:02:01)
        assert "04:03:02:01" in tx_data, (
            "Expected LD2412 command frame footer 04:03:02:01 in TX log"
        )

        # Recovery frame values (Phase 5, after overflow)
        assert len(sensor_states["moving_distance"]) >= 1, (
            f"Expected recovery moving_distance, got: {sensor_states['moving_distance']}"
        )
        # Find the recovery value (moving_distance = 50)
        recovery_values = [
            v for v in sensor_states["moving_distance"] if v == pytest.approx(50.0)
        ]
        assert len(recovery_values) >= 1, (
            f"Expected moving_distance=50 in recovery, got: {sensor_states['moving_distance']}"
        )

        # Recovery frame: moving=50, still=75, energy=100/80, detect=50
        recovery_idx = next(
            i
            for i, v in enumerate(sensor_states["moving_distance"])
            if v == pytest.approx(50.0)
        )
        assert sensor_states["still_distance"][recovery_idx] == pytest.approx(75.0), (
            f"Recovery still distance should be 75, got {sensor_states['still_distance'][recovery_idx]}"
        )
        assert sensor_states["moving_energy"][recovery_idx] == pytest.approx(100.0), (
            f"Recovery moving energy should be 100, got {sensor_states['moving_energy'][recovery_idx]}"
        )
        assert sensor_states["still_energy"][recovery_idx] == pytest.approx(80.0), (
            f"Recovery still energy should be 80, got {sensor_states['still_energy'][recovery_idx]}"
        )
        # LD2412 detection_distance = moving_distance when MOVE_BITMASK set
        assert sensor_states["detection_distance"][recovery_idx] == pytest.approx(
            50.0
        ), (
            f"Recovery detection distance should be 50, got {sensor_states['detection_distance'][recovery_idx]}"
        )

        # Verify binary sensors detected targets (from Phase 1 frame)
        assert len(binary_states["has_target"]) >= 1, "Expected has_target state"
        assert binary_states["has_target"][0] is True, "Has target should be True"
        assert len(binary_states["has_moving_target"]) >= 1, (
            "Expected has_moving_target state"
        )
        assert binary_states["has_moving_target"][0] is True, (
            "Has moving target should be True"
        )
        assert len(binary_states["has_still_target"]) >= 1, (
            "Expected has_still_target state"
        )
        assert binary_states["has_still_target"][0] is True, (
            "Has still target should be True"
        )


@pytest.mark.asyncio
async def test_uart_mock_ld2412_engineering(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2412 engineering mode with per-gate energy, light, and multi-byte distance."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()

    # Track sensor state updates (after initial state is swallowed)
    sensor_states: dict[str, list[float]] = {
        "moving_distance": [],
        "still_distance": [],
        "moving_energy": [],
        "still_energy": [],
        "detection_distance": [],
        "light": [],
        "gate_0_move_energy": [],
        "gate_1_move_energy": [],
        "gate_2_move_energy": [],
        "gate_0_still_energy": [],
        "gate_1_still_energy": [],
        "gate_2_still_energy": [],
    }
    binary_states: dict[str, list[bool]] = {
        "has_target": [],
        "has_moving_target": [],
        "has_still_target": [],
    }

    # Signal when we see Phase 1 frame values (moving_distance = 30)
    phase1_received = loop.create_future()
    # Signal when we see Phase 3 frame values
    phase3_still_received = loop.create_future()
    phase3_detect_received = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in sensor_states:
                sensor_states[sensor_name].append(state.state)
                if (
                    sensor_name == "moving_distance"
                    and state.state == pytest.approx(30.0)
                    and not phase1_received.done()
                ):
                    phase1_received.set_result(True)
                if (
                    sensor_name == "still_distance"
                    and state.state == pytest.approx(291.0)
                    and not phase3_still_received.done()
                ):
                    phase3_still_received.set_result(True)
                if (
                    sensor_name == "detection_distance"
                    and state.state == pytest.approx(291.0)
                    and not phase3_detect_received.done()
                ):
                    phase3_detect_received.set_result(True)
        elif isinstance(state, BinarySensorState):
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in binary_states:
                binary_states[sensor_name].append(state.state)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        all_names = list(sensor_states.keys()) + list(binary_states.keys())
        # Sort by descending length to avoid substring collisions
        # (e.g., "still_energy" matching "gate_0_still_energy")
        all_names.sort(key=len, reverse=True)
        key_to_sensor = build_key_to_entity_mapping(entities, all_names)

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

        # Wait for Phase 1 frame to be received
        try:
            await asyncio.wait_for(phase1_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 1 frame. Received:\n"
                f"  moving_distance: {sensor_states['moving_distance']}"
            )

        # Phase 1 values (engineering mode frame):
        # moving=30, energy=100, still=30, energy=100, detect=30
        assert sensor_states["moving_distance"][0] == pytest.approx(30.0), (
            f"Phase 1 moving distance should be 30, got {sensor_states['moving_distance'][0]}"
        )
        assert sensor_states["still_distance"][0] == pytest.approx(30.0), (
            f"Phase 1 still distance should be 30, got {sensor_states['still_distance'][0]}"
        )

        # Gate 0 moving energy = 0x64 = 100
        assert sensor_states["gate_0_move_energy"][0] == pytest.approx(100.0), (
            f"Gate 0 move energy should be 100, got {sensor_states['gate_0_move_energy'][0]}"
        )
        # Gate 1 moving energy = 0x41 = 65
        assert sensor_states["gate_1_move_energy"][0] == pytest.approx(65.0), (
            f"Gate 1 move energy should be 65, got {sensor_states['gate_1_move_energy'][0]}"
        )
        # Light sensor = 0x57 = 87
        assert sensor_states["light"][0] == pytest.approx(87.0), (
            f"Light sensor should be 87, got {sensor_states['light'][0]}"
        )

        # Wait for Phase 3 frame: still_distance = 291cm (multi-byte)
        try:
            await asyncio.wait_for(phase3_still_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 3 still_distance. Received:\n"
                f"  still_distance: {sensor_states['still_distance']}\n"
                f"  moving_distance: {sensor_states['moving_distance']}"
            )

        assert pytest.approx(291.0) in sensor_states["still_distance"], (
            f"Expected still_distance=291, got: {sensor_states['still_distance']}"
        )

        # Wait for Phase 3: detection_distance = 291 (still-only target)
        # target_state=0x02 so LD2412 uses still_distance for detection_distance.
        # The throttle_with_priority filter may delay this value.
        try:
            await asyncio.wait_for(phase3_detect_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for detection_distance=291 (still-only target). "
                f"Received: {sensor_states['detection_distance']}"
            )

        assert pytest.approx(291.0) in sensor_states["detection_distance"], (
            f"Expected detection_distance=291, got: {sensor_states['detection_distance']}"
        )
