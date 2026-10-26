"""Integration test for the LD2420 operating mode select with a mock UART.

The module runs firmware v2.0.0, so it starts in energy reporting. Choosing Simple
must write the simple system mode to the module and parse simple frames afterwards;
choosing Normal again must write the energy system mode back.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, SelectInfo
import pytest

from .log_utils import LineWaiter
from .state_utils import (
    SensorStateCollector,
    require_entity,
    subscribe_after_initial_states,
)
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

# CMD_WRITE_SYS_PARAM (0x0012), parameter 0x0000 (system mode), then the mode and 0x0000
WRITE_SIMPLE = "FD:FC:FB:FA:08:00:12:00:00:00:64:00:00:00:04:03:02:01"
WRITE_ENERGY = "FD:FC:FB:FA:08:00:12:00:00:00:04:00:00:00:04:03:02:01"


@pytest.mark.asyncio
async def test_uart_mock_ld2420_select_mode(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The select switches the module between energy and simple reporting."""
    waiter = LineWaiter()
    collector = SensorStateCollector(
        sensor_names=["moving_distance"], binary_sensor_names=["has_target"]
    )
    simple_frame_parsed = collector.add_waiter(
        lambda: pytest.approx(35.0) in collector.sensor_states["moving_distance"]
    )

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)
        await subscribe_after_initial_states(
            client, entities, on_state=collector.on_state
        )
        mode = require_entity(entities, "operating_mode", SelectInfo)
        # Setup wrote energy mode for this firmware
        await waiter.wait_for("TX", WRITE_ENERGY)

        def energy_writes() -> int:
            return sum(WRITE_ENERGY in line for line in waiter.lines)

        writes_before = energy_writes()

        client.select_command(mode.key, "Simple")
        await waiter.wait_for("TX", WRITE_SIMPLE)

        client.button_command(
            require_entity(entities, "start_scenario", ButtonInfo).key
        )
        await collector.wait_for_all(timeout=3.0)
        await simple_frame_parsed

        client.select_command(mode.key, "Normal")

        async def energy_written_again() -> None:
            while energy_writes() == writes_before:
                await asyncio.sleep(0.05)

        await wait_or_fail(
            energy_written_again(), 3.0, "Normal did not write energy mode"
        )
        assert energy_writes() == writes_before + 1
