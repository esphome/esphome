"""Integration test for LD2450 component with mock UART.

Tests:
test_uart_mock_ld2450:
  1. Happy path - valid periodic data frame publishes correct target sensor values
  2. Multi-target tracking - verifies target count, moving/still counts
  3. Target coordinate decoding - signed X/Y coordinates with sign-magnitude encoding
  4. Speed decoding - approaching (negative) and stationary (zero) targets
  5. Distance calculation - computed from X/Y via sqrt(x²+y²)
  6. Direction text sensor - "Approaching" for negative speed target
  7. Garbage resilience - random bytes don't crash the component
  8. Truncated frame handling - partial frame doesn't corrupt state
  9. Truncated frame recovery - the frame is dropped once its length is reached
  10. Post-garbage parsing - the next valid frame is parsed correctly
  11. TX logging - verifies LD2450 sends expected setup commands
"""

from __future__ import annotations

from aioesphomeapi import ButtonInfo
import pytest

from .log_utils import LineWaiter
from .state_utils import InitialStateHelper, SensorStateCollector, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_uart_mock_ld2450(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2450 data parsing with happy path, garbage, a truncated frame, and recovery."""
    waiter = LineWaiter()

    collector = SensorStateCollector(
        sensor_names=[
            "target_1_x",
            "target_1_y",
            "target_1_speed",
            "target_1_distance",
            "target_1_resolution",
            "target_1_angle",
            "target_2_x",
            "target_2_y",
            "target_2_speed",
            "target_2_distance",
            "target_count",
            "still_target_count",
            "moving_target_count",
        ],
        binary_sensor_names=[
            "has_target",
            "has_moving_target",
            "has_still_target",
        ],
        text_sensor_names=[
            "target_1_direction",
        ],
    )

    # Signal when we see all recovery frame values
    # Must wait for ALL values to avoid race where some arrive after the waiter fires
    recovery_received = collector.add_waiter(
        lambda: (
            pytest.approx(500.0, abs=1.0)
            in collector.sensor_states["target_1_distance"]
            and pytest.approx(300.0) in collector.sensor_states["target_1_x"]
            and pytest.approx(400.0) in collector.sensor_states["target_1_y"]
            and pytest.approx(30.0) in collector.sensor_states["target_1_speed"]
            and pytest.approx(1.0) in collector.sensor_states["target_count"]
            and pytest.approx(1.0) in collector.sensor_states["moving_target_count"]
            and pytest.approx(0.0) in collector.sensor_states["still_target_count"]
        )
    )

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
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
        await collector.wait_for_all(timeout=5.0)

        # Phase 1 values:
        # Target 1: X=-500, Y=1000, Speed=-50 (approaching), Res=320
        #   Distance = sqrt(500²+1000²) ≈ 1118mm
        assert collector.sensor_states["target_1_x"][0] == pytest.approx(-500.0)
        assert collector.sensor_states["target_1_y"][0] == pytest.approx(1000.0)
        assert collector.sensor_states["target_1_speed"][0] == pytest.approx(-50.0)
        assert collector.sensor_states["target_1_resolution"][0] == pytest.approx(320.0)
        # Distance computed from X/Y
        assert collector.sensor_states["target_1_distance"][0] == pytest.approx(
            1118.0, abs=1.0
        )

        # Target 2: X=200, Y=500, Speed=0 (stationary), Res=100
        #   Distance = sqrt(200²+500²) ≈ 538mm
        assert collector.sensor_states["target_2_x"][0] == pytest.approx(200.0)
        assert collector.sensor_states["target_2_y"][0] == pytest.approx(500.0)
        assert collector.sensor_states["target_2_speed"][0] == pytest.approx(0.0)
        assert collector.sensor_states["target_2_distance"][0] == pytest.approx(
            538.0, abs=1.0
        )

        # Target counts: 2 targets total, 1 moving, 1 still
        assert collector.sensor_states["target_count"][0] == pytest.approx(2.0)
        assert collector.sensor_states["moving_target_count"][0] == pytest.approx(1.0)
        assert collector.sensor_states["still_target_count"][0] == pytest.approx(1.0)

        # Binary sensors: all true (targets detected)
        assert collector.binary_states["has_target"][0] is True
        assert collector.binary_states["has_moving_target"][0] is True
        assert collector.binary_states["has_still_target"][0] is True

        # Direction text sensor: Target 1 is approaching (speed < 0)
        assert collector.text_sensor_states["target_1_direction"][0] == "Approaching"

        # Wait for the recovery frame (Phase 5) to be parsed
        # This proves the component survived garbage + truncated frame + filler bytes
        await wait_or_fail(
            recovery_received,
            5.0,
            "Timeout waiting for recovery frame",
            sensor_states=collector.sensor_states,
        )

        # Verify the truncated frame was dropped with a warning
        assert any("Invalid header/footer" in line for line in waiter.lines), (
            "Expected 'Invalid header/footer' warning in logs"
        )

        # Verify LD2450 sent setup commands (TX logging)
        tx_log_lines = [
            line for line in waiter.lines if "uart_mock" in line and "TX " in line
        ]
        assert len(tx_log_lines) > 0, "Expected TX log lines from uart_mock"
        tx_data = " ".join(tx_log_lines)
        # Verify command frame header appears (FD:FC:FB:FA)
        assert "FD:FC:FB:FA" in tx_data, (
            "Expected LD2450 command frame header FD:FC:FB:FA in TX log"
        )
        # Verify command frame footer appears (04:03:02:01)
        assert "04:03:02:01" in tx_data, (
            "Expected LD2450 command frame footer 04:03:02:01 in TX log"
        )

        # Recovery frame values (Phase 5, after the garbage):
        # Target 1: X=300, Y=400, Distance=500, Speed=30 (moving away)
        # target_count=1, moving=1, still=0
        #
        # Note: throttle filters cause sensor lists to have different lengths,
        # so we check each value appeared somewhere rather than using a shared index.
        assert (
            pytest.approx(500.0, abs=1.0)
            in collector.sensor_states["target_1_distance"]
        )
        assert pytest.approx(300.0) in collector.sensor_states["target_1_x"]
        assert pytest.approx(400.0) in collector.sensor_states["target_1_y"]
        assert pytest.approx(30.0) in collector.sensor_states["target_1_speed"]
        assert pytest.approx(1.0) in collector.sensor_states["target_count"]
        assert pytest.approx(1.0) in collector.sensor_states["moving_target_count"]
        assert pytest.approx(0.0) in collector.sensor_states["still_target_count"]
