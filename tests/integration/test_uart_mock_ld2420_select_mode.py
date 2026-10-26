"""Integration test for the LD2420 operating mode select with a mock UART.

The module runs firmware v2.0.0, so it starts in energy reporting. Choosing Simple
must write the simple system mode to the module and parse simple frames afterwards;
choosing Normal again must write the energy system mode back.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, EntityState, SelectInfo, SelectState
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
    collector = SensorStateCollector(sensor_names=["moving_distance"])
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
        writes_before = waiter.count(WRITE_ENERGY)

        client.select_command(mode.key, "Simple")
        await waiter.wait_for("TX", WRITE_SIMPLE)

        client.button_command(
            require_entity(entities, "start_scenario", ButtonInfo).key
        )
        await wait_or_fail(simple_frame_parsed, 3.0, "Simple frame was not parsed")

        client.select_command(mode.key, "Normal")
        await waiter.wait_count(
            WRITE_ENERGY, writes_before + 1, 3.0, "Normal did not write energy mode"
        )
        assert waiter.count(WRITE_ENERGY) == writes_before + 1


@pytest.mark.asyncio
async def test_uart_mock_ld2420_select_mode_nack(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A refused Simple write puts the select back on the mode the module reports."""
    waiter = LineWaiter()
    states: list[str] = []
    back_to_normal = asyncio.Event()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SelectState):
            states.append(state.state)
            if states[-2:] == ["Simple", "Normal"]:
                back_to_normal.set()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        await subscribe_after_initial_states(client, entities, on_state=on_state)
        mode = require_entity(entities, "operating_mode", SelectInfo)
        client.select_command(mode.key, "Simple")
        # Three tries of about a second each before the write is given up
        await wait_or_fail(back_to_normal, 6.0, "Select did not go back to Normal")
        assert waiter.count(WRITE_SIMPLE) == 3
