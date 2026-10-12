"""Integration test for LD2450 has_target without the sensor and number platforms.

Tests:
1. has_target turns on when only binary sensors are configured (target coordinates are still decoded)
2. has_target holds for the default presence_timeout of 5 s, without the presence_timeout number
"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import BinarySensorInfo, BinarySensorState, ButtonInfo, EntityState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

# The fixture sends one frame with a target, then 2 s of frames without one
SCENARIO_S = 2.1


@pytest.mark.asyncio
async def test_uart_mock_ld2450_has_target_only(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test has_target with no sensor platform and no presence_timeout number."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        has_target = require_entity(entities, "has_target", BinarySensorInfo)
        history: list[bool] = []
        turned_on = asyncio.get_running_loop().create_future()

        def on_state(state: EntityState) -> None:
            if isinstance(state, BinarySensorState) and state.key == has_target.key:
                history.append(state.state)
                if state.state and not turned_on.done():
                    turned_on.set_result(None)

        helper = InitialStateHelper(entities)
        client.subscribe_states(helper.on_state_wrapper(on_state))
        await helper.wait_for_initial_states()

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)
        await asyncio.wait_for(turned_on, timeout=5.0)
        await asyncio.sleep(SCENARIO_S)

        assert history == [True], (
            "has_target must turn on and hold through 2 s without targets, "
            f"got {history}"
        )
