"""Integration test for light::ToggleAction.

Tests both ToggleAction<HasTransitionLength=false> and
ToggleAction<HasTransitionLength=true> instantiations.
"""

import asyncio
from typing import Any

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_toggle_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test light.toggle with and without transition_length."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        state_futures: dict[int, asyncio.Future[Any]] = {}

        def on_state(state: Any) -> None:
            if state.key in state_futures and not state_futures[state.key].done():
                state_futures[state.key].set_result(state)

        client.subscribe_states(on_state)

        entities = await client.list_entities_services()
        light = next(e for e in entities[0] if e.object_id == "test_light")
        buttons = {e.name: e for e in entities[0] if hasattr(e, "name")}

        async def wait_for_state(key: int, timeout: float = 5.0) -> Any:
            loop = asyncio.get_running_loop()
            state_futures[key] = loop.create_future()
            try:
                return await asyncio.wait_for(state_futures[key], timeout)
            finally:
                state_futures.pop(key, None)

        async def press_and_wait(button_name: str) -> Any:
            btn = buttons[button_name]
            client.button_command(btn.key)
            return await wait_for_state(light.key)

        # Test 1: toggle without transition_length flips off->on
        state = await press_and_wait("Toggle")
        assert state.state is True

        # Test 2: toggle with transition_length flips on->off
        state = await press_and_wait("Toggle With Trans")
        assert state.state is False

        # Test 3: toggle without transition_length flips off->on again
        state = await press_and_wait("Toggle")
        assert state.state is True
