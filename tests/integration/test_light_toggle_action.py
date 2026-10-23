"""Integration test for the light.toggle action, with and without transition_length."""

from __future__ import annotations

from functools import partial

from aioesphomeapi import LightInfo, LightState
import pytest

from .state_utils import press_and_wait, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_toggle_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test light.toggle with and without transition_length."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sub = await subscribe_after_initial_states(client, entities)

        require_entity(entities, "test_light", LightInfo)

        press = partial(
            press_and_wait, client, entities, sub.waiter, state_type=LightState
        )

        # Test 1: toggle without transition_length flips off->on
        state = await press("toggle")
        assert state.state is True

        # Test 2: toggle with transition_length flips on->off
        state = await press("toggle_with_trans")
        assert state.state is False

        # Test 3: toggle without transition_length flips off->on again
        state = await press("toggle")
        assert state.state is True
