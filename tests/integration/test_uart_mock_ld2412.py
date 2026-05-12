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

test_uart_mock_ld2412_engineering_truncated (truncated engineering mode):
  1. Valid engineering frame establishes baseline sensor values
  2. Truncated engineering frame (24 bytes) is rejected — gate/light sensors
     must not receive garbage from stale buffer data or frame footer bytes
  3. Recovery frame with different values proves the component survived
"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import InitialStateHelper, SensorStateCollector, find_entity
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

    collector = SensorStateCollector(
        sensor_names=[
            "moving_distance",
            "still_distance",
            "moving_energy",
            "still_energy",
            "detection_distance",
        ],
        binary_sensor_names=[
            "has_target",
            "has_moving_target",
            "has_still_target",
        ],
    )

    # Signal when we see all recovery frame values
    recovery_received = collector.add_waiter(
        lambda: (
            pytest.approx(50.0) in collector.sensor_states["moving_distance"]
            and pytest.approx(75.0) in collector.sensor_states["still_distance"]
            and pytest.approx(100.0) in collector.sensor_states["moving_energy"]
            and pytest.approx(80.0) in collector.sensor_states["still_energy"]
            and pytest.approx(50.0) in collector.sensor_states["detection_distance"]
        )
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)

        # Set up initial state helper
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        # Wait for Phase 1 - all sensors and binary sensors have at least one value
        try:
            await collector.wait_for_all(timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 1 frame. Received:\n"
                f"  sensor_states: {collector.sensor_states}\n"
                f"  binary_states: {collector.binary_states}"
            )

        # Phase 1 values: moving=100, still=120, energy=50/25, detect=100
        assert collector.sensor_states["moving_distance"][0] == pytest.approx(100.0)
        assert collector.sensor_states["still_distance"][0] == pytest.approx(120.0)
        assert collector.sensor_states["moving_energy"][0] == pytest.approx(50.0)
        assert collector.sensor_states["still_energy"][0] == pytest.approx(25.0)
        assert collector.sensor_states["detection_distance"][0] == pytest.approx(100.0)

        # Wait for the recovery frame (Phase 5) to be parsed
        # This proves the component survived garbage + truncated + overflow
        try:
            await asyncio.wait_for(recovery_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for recovery frame. Received:\n"
                f"  sensor_states: {collector.sensor_states}"
            )

        # Verify overflow warning was logged
        assert overflow_seen.done(), (
            "Expected 'Max command length exceeded' warning in logs"
        )

        # Verify LD2412 sent setup commands (TX logging)
        assert len(tx_log_lines) > 0, "Expected TX log lines from uart_mock"
        tx_data = " ".join(tx_log_lines)
        assert "FD:FC:FB:FA" in tx_data, (
            "Expected LD2412 command frame header FD:FC:FB:FA in TX log"
        )
        assert "04:03:02:01" in tx_data, (
            "Expected LD2412 command frame footer 04:03:02:01 in TX log"
        )

        # Recovery frame: moving=50, still=75, energy=100/80, detect=50
        # Check values exist (waiter already ensured all are present)
        assert pytest.approx(50.0) in collector.sensor_states["moving_distance"]
        assert pytest.approx(75.0) in collector.sensor_states["still_distance"]
        assert pytest.approx(100.0) in collector.sensor_states["moving_energy"]
        assert pytest.approx(80.0) in collector.sensor_states["still_energy"]
        assert pytest.approx(50.0) in collector.sensor_states["detection_distance"]

        # Verify binary sensors detected targets (from Phase 1 frame)
        assert collector.binary_states["has_target"][0] is True
        assert collector.binary_states["has_moving_target"][0] is True
        assert collector.binary_states["has_still_target"][0] is True


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

    collector = SensorStateCollector(
        sensor_names=[
            "moving_distance",
            "still_distance",
            "moving_energy",
            "still_energy",
            "detection_distance",
            "light",
            "gate_0_move_energy",
            "gate_1_move_energy",
            "gate_2_move_energy",
            "gate_0_still_energy",
            "gate_1_still_energy",
            "gate_2_still_energy",
        ],
        binary_sensor_names=[
            "has_target",
            "has_moving_target",
            "has_still_target",
        ],
    )

    # Signal when we see Phase 3 frame values
    phase3_still_received = collector.add_waiter(
        lambda: pytest.approx(291.0) in collector.sensor_states["still_distance"]
    )
    phase3_detect_received = collector.add_waiter(
        lambda: pytest.approx(291.0) in collector.sensor_states["detection_distance"]
    )

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        # Wait for Phase 1 - all sensors and binary sensors have at least one value
        try:
            await collector.wait_for_all(timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 1 frame. Received:\n"
                f"  sensor_states: {collector.sensor_states}\n"
                f"  binary_states: {collector.binary_states}"
            )

        # Phase 1 values (engineering mode frame):
        # moving=30, energy=100, still=30, energy=100, detect=30
        assert collector.sensor_states["moving_distance"][0] == pytest.approx(30.0)
        assert collector.sensor_states["still_distance"][0] == pytest.approx(30.0)
        assert collector.sensor_states["gate_0_move_energy"][0] == pytest.approx(100.0)
        assert collector.sensor_states["gate_1_move_energy"][0] == pytest.approx(65.0)
        assert collector.sensor_states["light"][0] == pytest.approx(87.0)

        # Wait for Phase 3 frame: still_distance = 291cm (multi-byte)
        try:
            await asyncio.wait_for(phase3_still_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 3 still_distance. Received:\n"
                f"  still_distance: {collector.sensor_states['still_distance']}"
            )

        assert pytest.approx(291.0) in collector.sensor_states["still_distance"]

        # Wait for Phase 3: detection_distance = 291 (still-only target)
        try:
            await asyncio.wait_for(phase3_detect_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for detection_distance=291. "
                f"Received: {collector.sensor_states['detection_distance']}"
            )

        assert pytest.approx(291.0) in collector.sensor_states["detection_distance"]


@pytest.mark.asyncio
async def test_uart_mock_ld2412_engineering_truncated(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that truncated engineering mode frames don't corrupt sensor values.

    Without the fix, a 24-byte engineering mode frame passes the old buffer_pos_ >= 12
    check but reads indices 17-45 from stale buffer data, publishing garbage values
    (e.g. frame footer bytes 0xF8=248 as gate energy).
    """
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()

    # Track the truncated frame warning
    truncated_warning_seen = loop.create_future()

    def line_callback(line: str) -> None:
        if (
            "Engineering mode packet too short" in line
            and not truncated_warning_seen.done()
        ):
            truncated_warning_seen.set_result(True)

    collector = SensorStateCollector(
        sensor_names=[
            "moving_distance",
            "still_distance",
            "moving_energy",
            "still_energy",
            "detection_distance",
            "light",
            "gate_0_move_energy",
            "gate_0_still_energy",
        ],
        binary_sensor_names=[
            "has_target",
            "has_moving_target",
            "has_still_target",
        ],
    )

    # Signal when we see ALL Phase 3 recovery values to avoid race where some
    # arrive after the waiter fires but before we index into the lists
    recovery_received = collector.add_waiter(
        lambda: (
            pytest.approx(50.0) in collector.sensor_states["gate_0_move_energy"]
            and pytest.approx(42.0) in collector.sensor_states["light"]
        )
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)

        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        # Wait for Phase 1 — valid engineering frame establishes baseline
        try:
            await collector.wait_for_all(timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 1 frame. Received:\n"
                f"  sensor_states: {collector.sensor_states}\n"
                f"  binary_states: {collector.binary_states}"
            )

        # Phase 1 baseline: gate_0_move=100, light=87
        assert collector.sensor_states["gate_0_move_energy"][0] == pytest.approx(100.0)
        assert collector.sensor_states["light"][0] == pytest.approx(87.0)

        # Wait for Phase 3 recovery frame (gate_0_move=50)
        try:
            await asyncio.wait_for(recovery_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for recovery frame. Received:\n"
                f"  gate_0_move_energy: {collector.sensor_states['gate_0_move_energy']}\n"
                f"  light: {collector.sensor_states['light']}"
            )

        # Verify the truncated frame warning was logged
        assert truncated_warning_seen.done(), (
            "Expected 'Engineering mode packet too short' warning in logs"
        )

        # Phase 3 recovery: gate_0_move=50, light=42
        assert pytest.approx(50.0) in collector.sensor_states["gate_0_move_energy"]
        assert pytest.approx(42.0) in collector.sensor_states["light"]

        # The critical assertion: gate_0_move_energy must never have received
        # garbage values from the truncated frame. Without the fix,
        # buffer_data_[17] = 0xFF = 255 would be published as gate_0_move.
        for value in collector.sensor_states["gate_0_move_energy"]:
            assert value == pytest.approx(100.0) or value == pytest.approx(50.0), (
                f"gate_0_move_energy got unexpected value {value} — "
                f"truncated frame likely leaked stale buffer data. "
                f"All values: {collector.sensor_states['gate_0_move_energy']}"
            )
