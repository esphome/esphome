"""Integration test for the light.toggle action, with and without transition_length."""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, EntityState, LightInfo, LightState
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_toggle_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test light.toggle with and without transition_length."""
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        light_state_future: asyncio.Future[LightState] | None = None

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, LightState)
                and light_state_future is not None
                and not light_state_future.done()
            ):
                light_state_future.set_result(state)

        async def wait_for_light_state(timeout: float = 5.0) -> LightState:
            nonlocal light_state_future
            light_state_future = loop.create_future()
            try:
                return await asyncio.wait_for(light_state_future, timeout)
            finally:
                light_state_future = None

        entities, _ = await client.list_entities_services()
        await subscribe_after_initial_states(client, entities, on_state=on_state)

        require_entity(entities, "test_light", LightInfo)

        async def press_and_wait(name: str) -> LightState:
            btn = require_entity(entities, name.lower().replace(" ", "_"), ButtonInfo)
            client.button_command(btn.key)
            return await wait_for_light_state()

        # Test 1: toggle without transition_length flips off->on
        state = await press_and_wait("Toggle")
        assert state.state is True

        # Test 2: toggle with transition_length flips on->off
        state = await press_and_wait("Toggle With Trans")
        assert state.state is False

        # Test 3: toggle without transition_length flips off->on again
        state = await press_and_wait("Toggle")
        assert state.state is True
