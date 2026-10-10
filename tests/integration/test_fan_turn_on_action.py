"""Integration test for fan TurnOnAction.

Tests that fan.turn_on automation actions work correctly across multiple
field combinations and the lambda path.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, EntityState, FanDirection, FanInfo, FanState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_fan_turn_on_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test fan TurnOnAction with constants and a lambda."""
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        fan_state_future: asyncio.Future[FanState] | None = None

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, FanState)
                and fan_state_future is not None
                and not fan_state_future.done()
            ):
                fan_state_future.set_result(state)

        async def wait_for_fan_state(timeout: float = 5.0) -> FanState:
            nonlocal fan_state_future
            fan_state_future = loop.create_future()
            try:
                return await asyncio.wait_for(fan_state_future, timeout)
            finally:
                fan_state_future = None

        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        require_entity(entities, "test_fan", FanInfo)

        async def press_and_wait(name: str) -> FanState:
            btn = require_entity(entities, name.lower().replace(" ", "_"), ButtonInfo)
            client.button_command(btn.key)
            return await wait_for_fan_state()

        # speed only
        state = await press_and_wait("Set Speed")
        assert state.state is True
        assert state.speed_level == 3

        # oscillating + direction
        state = await press_and_wait("Set Oscillate Direction")
        assert state.oscillating is True
        assert state.direction == FanDirection.REVERSE

        # all three fields
        state = await press_and_wait("Set All Fields")
        assert state.oscillating is False
        assert state.speed_level == 4
        assert state.direction == FanDirection.FORWARD

        # lambda path: speed computed at runtime (test_speed global = 2)
        state = await press_and_wait("Lambda Speed")
        assert state.speed_level == 2
