"""Integration test for light::DimRelativeAction.

Tests both DimRelativeAction<HasTransitionLength=false> and
DimRelativeAction<HasTransitionLength=true> instantiations.
"""

import asyncio
from typing import Any

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_dim_relative_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test light.dim_relative with and without transition_length."""
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
