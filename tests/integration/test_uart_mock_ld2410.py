"""Integration test for LD2410 component with mock UART.

Tests:
test_uart_mock_ld2410 (normal mode):
  1. Happy path - valid data frame publishes correct sensor values
  2. Garbage resilience - random bytes don't crash the component
  3. Truncated frame handling - partial frame doesn't corrupt state
  4. Truncated frame recovery - the frame is dropped once its length is reached
  5. Post-garbage parsing - the next valid frame is parsed correctly
  6. Footer bytes in the values - a frame whose values spell the command footer is parsed whole
  7. Replay after a drop - a valid frame caught inside a dropped frame's window is still parsed
  8. Impossible counts - a length field of 0xFFF6 or 0xFFFF is dropped and the next frame is parsed
  9. TX logging - verifies LD2410 sends expected setup commands

test_uart_mock_ld2410_engineering (engineering mode):
  1. Engineering mode frames with per-gate energy data and light sensor
  2. Multi-byte still distance (291cm) using high byte > 0
  3. Out pin presence binary sensor
  4. Gate energy sensor values from real device captures
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import InitialStateHelper, SensorStateCollector, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_uart_mock_ld2410(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2410 data parsing with happy path, garbage, a truncated frame, and recovery."""
    loop = asyncio.get_running_loop()

    # Track the warning for the dropped truncated frame in logs
    resync_seen = loop.create_future()

    # Track TX data logged by the mock for assertions
    tx_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "Invalid length" in line and not resync_seen.done():
            resync_seen.set_result(True)
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

    # Signal when we see ALL recovery frame values to avoid race where some
    # arrive after the waiter fires but before we index into the lists
    recovery_received = collector.add_waiter(
        lambda: (
            pytest.approx(50.0) in collector.sensor_states["moving_distance"]
            and pytest.approx(75.0) in collector.sensor_states["still_distance"]
            and pytest.approx(100.0) in collector.sensor_states["moving_energy"]
            and pytest.approx(80.0) in collector.sensor_states["still_energy"]
            and pytest.approx(127.0) in collector.sensor_states["detection_distance"]
            # Every value asserted below, so no state can still be in flight when the waiter fires
            and pytest.approx(772.0) in collector.sensor_states["moving_distance"]
            and pytest.approx(2.0) in collector.sensor_states["moving_energy"]
            and pytest.approx(1.0) in collector.sensor_states["still_distance"]
            and pytest.approx(5.0) in collector.sensor_states["still_energy"]
            and pytest.approx(772.0) in collector.sensor_states["detection_distance"]
            and pytest.approx(400.0) in collector.sensor_states["moving_distance"]
            and pytest.approx(7.0) in collector.sensor_states["moving_energy"]
            and pytest.approx(200.0) in collector.sensor_states["still_distance"]
            and pytest.approx(9.0) in collector.sensor_states["still_energy"]
            and pytest.approx(400.0) in collector.sensor_states["detection_distance"]
            and pytest.approx(600.0) in collector.sensor_states["moving_distance"]
            and pytest.approx(11.0) in collector.sensor_states["moving_energy"]
            and pytest.approx(300.0) in collector.sensor_states["still_distance"]
            and pytest.approx(13.0) in collector.sensor_states["still_energy"]
            and pytest.approx(600.0) in collector.sensor_states["detection_distance"]
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

        await initial_state_helper.wait_for_initial_states()

        # Start the UART mock scenario now that we're subscribed
        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)

        # Wait for Phase 1 - all sensors and binary sensors have at least one value
        await collector.wait_for_all(timeout=3.0)

        # Phase 1 values: moving=100, still=120, energy=50/25, detect=300
        assert collector.sensor_states["moving_distance"][0] == pytest.approx(100.0)
        assert collector.sensor_states["still_distance"][0] == pytest.approx(120.0)
        assert collector.sensor_states["moving_energy"][0] == pytest.approx(50.0)
        assert collector.sensor_states["still_energy"][0] == pytest.approx(25.0)
        assert collector.sensor_states["detection_distance"][0] == pytest.approx(300.0)

        # Wait for the recovery frame (Phase 5) to be parsed
        # This proves the component survived garbage + truncated frame + filler bytes
        await wait_or_fail(
            recovery_received,
            15.0,
            "Timeout waiting for recovery frame",
            sensor_states=collector.sensor_states,
        )

        # Verify the truncated frame was dropped with a warning
        assert resync_seen.done(), "Expected 'Invalid length' warning in logs"

        # Verify LD2410 sent setup commands (TX logging)
        # LD2410 sends 7 commands during setup: FF (config on), A0 (version),
        # A5 (MAC), AB (distance res), AE (light), 61 (params), FE (config off)
        assert len(tx_log_lines) > 0, "Expected TX log lines from uart_mock"
        tx_data = " ".join(tx_log_lines)
        assert "FD:FC:FB:FA" in tx_data, (
            "Expected LD2410 command frame header FD:FC:FB:FA in TX log"
        )
        assert "04:03:02:01" in tx_data, (
            "Expected LD2410 command frame footer 04:03:02:01 in TX log"
        )

        # Recovery frame: moving=50, still=75, energy=100/80, detect=127
        recovery_idx = next(
            i
            for i, v in enumerate(collector.sensor_states["moving_distance"])
            if v == pytest.approx(50.0)
        )
        assert collector.sensor_states["still_distance"][recovery_idx] == pytest.approx(
            75.0
        )
        assert collector.sensor_states["moving_energy"][recovery_idx] == pytest.approx(
            100.0
        )
        assert collector.sensor_states["still_energy"][recovery_idx] == pytest.approx(
            80.0
        )
        assert collector.sensor_states["detection_distance"][
            recovery_idx
        ] == pytest.approx(127.0)

        # Phase 6: the values 04 03 02 01 did not end the frame early
        assert pytest.approx(772.0) in collector.sensor_states["moving_distance"]
        assert pytest.approx(2.0) in collector.sensor_states["moving_energy"]
        assert pytest.approx(1.0) in collector.sensor_states["still_distance"]
        assert pytest.approx(5.0) in collector.sensor_states["still_energy"]
        assert pytest.approx(772.0) in collector.sensor_states["detection_distance"]

        # Phase 7: the valid frame inside the dropped frame's window was replayed and parsed
        assert pytest.approx(400.0) in collector.sensor_states["moving_distance"]
        assert pytest.approx(7.0) in collector.sensor_states["moving_energy"]
        assert pytest.approx(200.0) in collector.sensor_states["still_distance"]
        assert pytest.approx(9.0) in collector.sensor_states["still_energy"]
        assert pytest.approx(400.0) in collector.sensor_states["detection_distance"]

        # Phase 8: both impossible counts were dropped and the frame after them was parsed
        assert pytest.approx(600.0) in collector.sensor_states["moving_distance"]
        assert pytest.approx(11.0) in collector.sensor_states["moving_energy"]
        assert pytest.approx(300.0) in collector.sensor_states["still_distance"]
        assert pytest.approx(13.0) in collector.sensor_states["still_energy"]
        assert pytest.approx(600.0) in collector.sensor_states["detection_distance"]

        # Verify binary sensors detected targets (from Phase 1 frame)
        assert collector.binary_states["has_target"][0] is True
        assert collector.binary_states["has_moving_target"][0] is True
        assert collector.binary_states["has_still_target"][0] is True


@pytest.mark.asyncio
async def test_uart_mock_ld2410_engineering(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2410 engineering mode with per-gate energy, light, and multi-byte distance."""
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
            "out_pin_presence",
        ],
    )

    # Signal when we see Phase 3 frame values
    phase3_received = collector.add_waiter(
        lambda: pytest.approx(291.0) in collector.sensor_states["still_distance"]
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

        await initial_state_helper.wait_for_initial_states()

        # Start the UART mock scenario now that we're subscribed
        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)

        # Wait for Phase 1 - all sensors and binary sensors have at least one value
        await collector.wait_for_all(timeout=3.0)

        # Phase 1 values (engineering mode frame):
        # moving=30, energy=100, still=30, energy=100, detect=0
        assert collector.sensor_states["moving_distance"][0] == pytest.approx(30.0)
        assert collector.sensor_states["still_distance"][0] == pytest.approx(30.0)
        assert collector.sensor_states["gate_0_move_energy"][0] == pytest.approx(100.0)
        assert collector.sensor_states["gate_1_move_energy"][0] == pytest.approx(65.0)
        assert collector.sensor_states["light"][0] == pytest.approx(87.0)
        assert collector.binary_states["out_pin_presence"][0] is True

        # Wait for Phase 3 frame (still_distance = 291cm, multi-byte)
        await wait_or_fail(
            phase3_received,
            15.0,
            "Timeout waiting for Phase 3 frame",
            still_distance=collector.sensor_states["still_distance"],
        )

        assert pytest.approx(291.0) in collector.sensor_states["still_distance"]
