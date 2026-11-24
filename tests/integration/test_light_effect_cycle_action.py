"""Integration test for light.effect.next and light.effect.previous."""

from __future__ import annotations

from functools import partial

from aioesphomeapi import LightInfo, LightState
import pytest

from .state_utils import press_and_wait, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_effect_cycle_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Cycle both ways through two effects, with and without "None"."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sub = await subscribe_after_initial_states(client, entities)

        require_entity(entities, "test_light", LightInfo)

        press = partial(
            press_and_wait, client, entities, sub.waiter, state_type=LightState
        )

        state = await press("setup")
        assert state.state is True
        assert state.effect == "None"

        # Without "None": wraps from the last effect to the first and back
        for button, effect in (
            ("next_skip", "A"),
            ("next_skip", "B"),
            ("next_skip", "A"),
            ("prev_skip", "B"),
            ("prev_skip", "A"),
            # With "None": it sits between the last and the first effect
            ("prev_none", "None"),
            ("prev_none", "B"),
            ("next_none", "None"),
            ("next_none", "A"),
        ):
            state = await press(button)
            assert state.effect == effect, button
