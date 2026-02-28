"""Integration test for LD2410 component with mock UART.

Tests:
test_uart_mock_ld2410 (normal mode):
  1. Happy path - valid data frame publishes correct sensor values
  2. Garbage resilience - random bytes don't crash the component
  3. Truncated frame handling - partial frame doesn't corrupt state
  4. Buffer overflow recovery - overflow resets the parser
  5. Post-overflow parsing - next valid frame after overflow is parsed correctly
  6. TX logging - verifies LD2410 sends expected setup commands

test_uart_mock_ld2410_engineering (engineering mode):
  1. Engineering mode frames with per-gate energy data and light sensor
  2. Multi-byte still distance (291cm) using high byte > 0
  3. Out pin presence binary sensor
  4. Gate energy sensor values from real device captures
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
async def test_uart_mock_ld2410(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2410 data parsing with happy path, garbage, overflow, and recovery."""
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

        still_dist_entity = find_entity(entities, "still_distance", SensorInfo)
        assert still_dist_entity is not None
        initial_still = initial_state_helper.initial_states.get(still_dist_entity.key)
        assert initial_still is not None and isinstance(initial_still, SensorState)
        assert initial_still.state == pytest.approx(120.0), (
            f"Initial still distance should be 120, got {initial_still.state}"
        )

        moving_energy_entity = find_entity(entities, "moving_energy", SensorInfo)
        assert moving_energy_entity is not None
        initial_me = initial_state_helper.initial_states.get(moving_energy_entity.key)
        assert initial_me is not None and isinstance(initial_me, SensorState)
        assert initial_me.state == pytest.approx(50.0), (
            f"Initial moving energy should be 50, got {initial_me.state}"
        )

        still_energy_entity = find_entity(entities, "still_energy", SensorInfo)
        assert still_energy_entity is not None
        initial_se = initial_state_helper.initial_states.get(still_energy_entity.key)
        assert initial_se is not None and isinstance(initial_se, SensorState)
        assert initial_se.state == pytest.approx(25.0), (
            f"Initial still energy should be 25, got {initial_se.state}"
        )

        detect_dist_entity = find_entity(entities, "detection_distance", SensorInfo)
        assert detect_dist_entity is not None
        initial_dd = initial_state_helper.initial_states.get(detect_dist_entity.key)
        assert initial_dd is not None and isinstance(initial_dd, SensorState)
        assert initial_dd.state == pytest.approx(300.0), (
            f"Initial detection distance should be 300, got {initial_dd.state}"
        )

        # Wait for the recovery frame (Phase 5) to be parsed
        # This proves the component survived garbage + truncated + overflow
        try:
            await asyncio.wait_for(recovery_received, timeout=15.0)
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

        # Verify LD2410 sent setup commands (TX logging)
        # LD2410 sends 7 commands during setup: FF (config on), A0 (version),
        # A5 (MAC), AB (distance res), AE (light), 61 (params), FE (config off)
        assert len(tx_log_lines) > 0, "Expected TX log lines from uart_mock"
        tx_data = " ".join(tx_log_lines)
        # Verify command frame header appears (FD:FC:FB:FA)
        assert "FD:FC:FB:FA" in tx_data, (
            "Expected LD2410 command frame header FD:FC:FB:FA in TX log"
        )
        # Verify command frame footer appears (04:03:02:01)
        assert "04:03:02:01" in tx_data, (
            "Expected LD2410 command frame footer 04:03:02:01 in TX log"
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

        # Recovery frame: moving=50, still=75, energy=100/80, detect=127
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
        assert sensor_states["detection_distance"][recovery_idx] == pytest.approx(
            127.0
        ), (
            f"Recovery detection distance should be 127, got {sensor_states['detection_distance'][recovery_idx]}"
        )

        # Verify binary sensors detected targets
        # Binary sensors could be in initial states or forwarded states
        has_target_entity = find_entity(entities, "has_target", BinarySensorInfo)
        assert has_target_entity is not None
        initial_ht = initial_state_helper.initial_states.get(has_target_entity.key)
        assert initial_ht is not None and isinstance(initial_ht, BinarySensorState)
        assert initial_ht.state is True, "Has target should be True"

        has_moving_entity = find_entity(entities, "has_moving_target", BinarySensorInfo)
        assert has_moving_entity is not None
        initial_hm = initial_state_helper.initial_states.get(has_moving_entity.key)
        assert initial_hm is not None and isinstance(initial_hm, BinarySensorState)
        assert initial_hm.state is True, "Has moving target should be True"

        has_still_entity = find_entity(entities, "has_still_target", BinarySensorInfo)
        assert has_still_entity is not None
        initial_hs = initial_state_helper.initial_states.get(has_still_entity.key)
        assert initial_hs is not None and isinstance(initial_hs, BinarySensorState)
        assert initial_hs.state is True, "Has still target should be True"


@pytest.mark.asyncio
async def test_uart_mock_ld2410_engineering(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2410 engineering mode with per-gate energy, light, and multi-byte distance."""
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
        "out_pin_presence": [],
    }

    # Signal when we see Phase 3 frame (still_distance = 291)
    phase3_received = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in sensor_states:
                sensor_states[sensor_name].append(state.state)
                if (
                    sensor_name == "still_distance"
                    and state.state == pytest.approx(291.0)
                    and not phase3_received.done()
                ):
                    phase3_received.set_result(True)
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
        key_to_sensor = build_key_to_entity_mapping(entities, all_names)

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Phase 1 initial values (engineering mode frame):
        # moving=30, energy=100, still=30, energy=100, detect=0
        moving_dist_entity = find_entity(entities, "moving_distance", SensorInfo)
        assert moving_dist_entity is not None
        initial_moving = initial_state_helper.initial_states.get(moving_dist_entity.key)
        assert initial_moving is not None and isinstance(initial_moving, SensorState)
        assert initial_moving.state == pytest.approx(30.0), (
            f"Initial moving distance should be 30, got {initial_moving.state}"
        )

        still_dist_entity = find_entity(entities, "still_distance", SensorInfo)
        assert still_dist_entity is not None
        initial_still = initial_state_helper.initial_states.get(still_dist_entity.key)
        assert initial_still is not None and isinstance(initial_still, SensorState)
        assert initial_still.state == pytest.approx(30.0), (
            f"Initial still distance should be 30, got {initial_still.state}"
        )

        # Verify engineering mode sensors from initial state
        # Gate 0 moving energy = 0x64 = 100
        gate0_move_entity = find_entity(entities, "gate_0_move_energy", SensorInfo)
        assert gate0_move_entity is not None
        initial_g0m = initial_state_helper.initial_states.get(gate0_move_entity.key)
        assert initial_g0m is not None and isinstance(initial_g0m, SensorState)
        assert initial_g0m.state == pytest.approx(100.0), (
            f"Gate 0 move energy should be 100, got {initial_g0m.state}"
        )

        # Gate 1 moving energy = 0x41 = 65
        gate1_move_entity = find_entity(entities, "gate_1_move_energy", SensorInfo)
        assert gate1_move_entity is not None
        initial_g1m = initial_state_helper.initial_states.get(gate1_move_entity.key)
        assert initial_g1m is not None and isinstance(initial_g1m, SensorState)
        assert initial_g1m.state == pytest.approx(65.0), (
            f"Gate 1 move energy should be 65, got {initial_g1m.state}"
        )

        # Light sensor = 0x57 = 87
        light_entity = find_entity(entities, "light", SensorInfo)
        assert light_entity is not None
        initial_light = initial_state_helper.initial_states.get(light_entity.key)
        assert initial_light is not None and isinstance(initial_light, SensorState)
        assert initial_light.state == pytest.approx(87.0), (
            f"Light sensor should be 87, got {initial_light.state}"
        )

        # Out pin presence = 0x01 = True
        out_pin_entity = find_entity(entities, "out_pin_presence", BinarySensorInfo)
        assert out_pin_entity is not None
        initial_out = initial_state_helper.initial_states.get(out_pin_entity.key)
        assert initial_out is not None and isinstance(initial_out, BinarySensorState)
        assert initial_out.state is True, "Out pin presence should be True"

        # Wait for Phase 3 frame (still_distance = 291cm, multi-byte)
        try:
            await asyncio.wait_for(phase3_received, timeout=15.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 3 frame. Received sensor states:\n"
                f"  still_distance: {sensor_states['still_distance']}\n"
                f"  moving_distance: {sensor_states['moving_distance']}"
            )

        # Phase 3: still distance = 0x0123 = 291cm (multi-byte distance test)
        phase3_still = [
            v for v in sensor_states["still_distance"] if v == pytest.approx(291.0)
        ]
        assert len(phase3_still) >= 1, (
            f"Expected still_distance=291, got: {sensor_states['still_distance']}"
        )
