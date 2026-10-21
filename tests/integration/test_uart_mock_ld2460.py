"""Integration test for the LD2460 component with a mock UART.

The frames come from the HLK-LD2460 serial protocol v1.0: the report example in
table 2 (one target at 1.5 m, 2.3 m), the installation-mode query in table 11 and
the receipts in table 12.

test_uart_mock_ld2460:
  1. The hub queries the installation mode at boot (TX logged by the mock)
  2. The datasheet report example decodes to x, y, distance and angle (side mount)
  3. Garbage, a truncated frame and a buffer overflow do not stop the parser
  4. A two-target frame after the overflow decodes, including a negative X
  5. A top-mount receipt switches the angle formula for the next frame
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import InitialStateHelper, SensorStateCollector, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


@pytest.mark.asyncio
async def test_uart_mock_ld2460(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2460 frame parsing with the datasheet frames, garbage and overflow."""
    loop = asyncio.get_running_loop()
    overflow_seen = loop.create_future()
    tx_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "Max frame length exceeded" in line and not overflow_seen.done():
            overflow_seen.set_result(True)
        if "uart_mock" in line and "TX " in line:
            tx_log_lines.append(line)

    collector = SensorStateCollector(
        sensor_names=[
            "target_count",
            "target_1_x",
            "target_1_y",
            "target_1_distance",
            "target_1_angle",
            "target_2_x",
            "target_2_y",
            "target_2_distance",
            "target_2_angle",
        ],
    )

    # Phase 5: two targets after the overflow
    recovery_received = collector.add_waiter(
        lambda: (
            pytest.approx(2.0) in collector.sensor_states["target_count"]
            and pytest.approx(-2.0, abs=0.01) in collector.sensor_states["target_1_x"]
            and pytest.approx(3.0, abs=0.01) in collector.sensor_states["target_1_y"]
            and pytest.approx(0.5, abs=0.01) in collector.sensor_states["target_2_x"]
            and pytest.approx(1.0, abs=0.01) in collector.sensor_states["target_2_y"]
            and pytest.approx(1.118, abs=0.01)
            in collector.sensor_states["target_2_distance"]
        )
    )
    # Phase 6: the same target as phase 1, measured as a top mount
    top_mount_received = collector.add_waiter(
        lambda: (
            pytest.approx(56.9, abs=0.1) in collector.sensor_states["target_1_angle"]
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

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)

        # Phase 1: the datasheet example, one target at (1.5, 2.3)
        phase1 = collector.add_waiter(
            lambda: (
                pytest.approx(1.0) in collector.sensor_states["target_count"]
                and pytest.approx(33.1, abs=0.1)
                in collector.sensor_states["target_1_angle"]
            )
        )
        await wait_or_fail(
            phase1,
            5.0,
            "Timeout waiting for the phase 1 frame",
            sensor_states=collector.sensor_states,
        )
        assert collector.sensor_states["target_1_x"][0] == pytest.approx(1.5, abs=0.01)
        assert collector.sensor_states["target_1_y"][0] == pytest.approx(2.3, abs=0.01)
        assert collector.sensor_states["target_1_distance"][0] == pytest.approx(
            2.746, abs=0.01
        )
        assert collector.sensor_states["target_1_angle"][0] == pytest.approx(
            33.1, abs=0.1
        )
        assert collector.sensor_states["target_count"][0] == pytest.approx(1.0)

        # Phase 5: parsing survives garbage, a truncated frame and an overflow
        await wait_or_fail(
            recovery_received,
            5.0,
            "Timeout waiting for the recovery frame",
            sensor_states=collector.sensor_states,
        )
        assert overflow_seen.done(), "Expected 'Max frame length exceeded' in the log"
        assert (
            pytest.approx(3.606, abs=0.01)
            in collector.sensor_states["target_1_distance"]
        )
        assert (
            pytest.approx(-33.7, abs=0.1) in collector.sensor_states["target_1_angle"]
        )
        assert pytest.approx(26.6, abs=0.1) in collector.sensor_states["target_2_angle"]

        # Phase 6: the top-mount receipt changes the angle for the same coordinates
        await wait_or_fail(
            top_mount_received,
            5.0,
            "Timeout waiting for the top-mount angle",
            sensor_states=collector.sensor_states,
        )

        # The hub sent the installation-mode query from the datasheet (table 11)
        tx_data = " ".join(tx_log_lines)
        assert "FD:FC:FB:FA:0A:0C:00:01:04:03:02:01" in tx_data, (
            f"Expected the installation-mode query in the TX log, got: {tx_data}"
        )
