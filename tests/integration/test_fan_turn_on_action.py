"""Integration test for fan TurnOnAction.

Tests that fan.turn_on automation actions work correctly across multiple
field combinations and the lambda path.
"""

from __future__ import annotations

from functools import partial

from aioesphomeapi import FanDirection, FanInfo, FanState
import pytest

from .state_utils import press_and_wait, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_fan_turn_on_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test fan TurnOnAction with constants and a lambda."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sub = await subscribe_after_initial_states(client, entities)

        require_entity(entities, "test_fan", FanInfo)

        press = partial(
            press_and_wait, client, entities, sub.waiter, state_type=FanState
        )

        # speed only
        state = await press("set_speed")
        assert state.state is True
        assert state.speed_level == 3

        # oscillating + direction
        state = await press("set_oscillate_direction")
        assert state.oscillating is True
        assert state.direction == FanDirection.REVERSE

        # all three fields
        state = await press("set_all_fields")
        assert state.oscillating is False
        assert state.speed_level == 4
        assert state.direction == FanDirection.FORWARD

        # lambda path: speed computed at runtime (test_speed global = 2)
        state = await press("lambda_speed")
        assert state.speed_level == 2
