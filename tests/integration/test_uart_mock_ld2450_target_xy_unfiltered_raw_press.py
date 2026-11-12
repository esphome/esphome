"""Integration test for pressing the LD2450 xy_unfiltered button from an xy sensor's on_raw_value automation."""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, EntityState, TextSensorInfo, TextSensorState
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction

# The positions of target 1 in the fixture, one per radar frame (100 ms apart)
POSITIONS = [f"{-600 + 100 * i},{1000 + 50 * i}" for i in range(4)]


@pytest.mark.asyncio
async def test_uart_mock_ld2450_target_xy_unfiltered_raw_press(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The button pressed while the sensor publishes does not crash, and the bypass starts."""
    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        target_1 = require_entity(entities, "target_1_xy", TextSensorInfo)
        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        history: list[str] = []
        target_left = asyncio.get_running_loop().create_future()

        def on_state(state: EntityState) -> None:
            if not isinstance(state, TextSensorState) or state.key != target_1.key:
                return
            history.append(state.state)
            if state.state == "NA" and not target_left.done():
                target_left.set_result(None)

        await subscribe_after_initial_states(client, entities, on_state=on_state)

        client.button_command(start_btn.key)
        await asyncio.wait_for(target_left, timeout=10.0)

        # The first position passes the filters, which the bypass replaces from the next loop on; the default
        # filters alone would have held back all the others
        assert history == [*POSITIONS, "NA"]
