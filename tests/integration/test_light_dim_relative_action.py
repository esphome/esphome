"""Integration test for light.dim_relative, with and without a transition length and limits."""

from __future__ import annotations

from functools import partial

from aioesphomeapi import LightInfo, LightState
import pytest

from .state_utils import press_and_wait, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_dim_relative_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test light.dim_relative with and without transition_length."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sub = await subscribe_after_initial_states(client, entities)

        require_entity(entities, "test_light", LightInfo)

        press = partial(
            press_and_wait, client, entities, sub.waiter, state_type=LightState
        )

        # Setup: turn on at 50%
        state = await press("setup")
        assert state.state is True
        assert state.brightness == pytest.approx(0.5, abs=0.05)

        # Test 1: dim_relative without transition_length: 50% + 25% = 75%
        state = await press("dim_up")
        assert state.brightness == pytest.approx(0.75, abs=0.05)

        # Test 2: dim_relative with transition_length: 75% - 10% = 65%
        state = await press("dim_down")
        assert state.brightness == pytest.approx(0.65, abs=0.05)

        # Test 3: dim_relative with max_brightness limit: 65% + 50% clamped to 80%
        state = await press("dim_clamp")
        assert state.brightness == pytest.approx(0.80, abs=0.05)

        # Test 4: 80% is below the 90% minimum, so DO_NOTHING skips the +10%
        # and the -5% after it still runs: 75%
        state = await press("dim_skip")
        assert state.brightness == pytest.approx(0.75, abs=0.05)
