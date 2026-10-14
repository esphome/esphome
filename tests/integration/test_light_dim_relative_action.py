"""Integration test for light::DimRelativeAction.

Tests both DimRelativeAction<HasTransitionLength=false> and
DimRelativeAction<HasTransitionLength=true> instantiations.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, EntityState, LightInfo, LightState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_dim_relative_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test light.dim_relative with and without transition_length."""
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
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        require_entity(entities, "test_light", LightInfo)

        async def press_and_wait(name: str) -> LightState:
            btn = require_entity(entities, name.lower().replace(" ", "_"), ButtonInfo)
            client.button_command(btn.key)
            return await wait_for_light_state()

        # Setup: turn on at 50%
        state = await press_and_wait("Setup")
        assert state.state is True
        assert state.brightness == pytest.approx(0.5, abs=0.05)

        # Test 1: dim_relative without transition_length: 50% + 25% = 75%
        state = await press_and_wait("Dim Up")
        assert state.brightness == pytest.approx(0.75, abs=0.05)

        # Test 2: dim_relative with transition_length: 75% - 10% = 65%
        state = await press_and_wait("Dim Down")
        assert state.brightness == pytest.approx(0.65, abs=0.05)

        # Test 3: dim_relative with max_brightness limit: 65% + 50% clamped to 80%
        state = await press_and_wait("Dim Clamp")
        assert state.brightness == pytest.approx(0.80, abs=0.05)
