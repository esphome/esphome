"""Integration test for valve ControlAction.

Tests that valve.control automation actions work correctly across multiple
field combinations and the lambda path.
"""

from __future__ import annotations

from functools import partial

from aioesphomeapi import ValveInfo, ValveOperation, ValveState
import pytest

from .state_utils import press_and_wait, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_valve_control_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test valve ControlAction with constants and a lambda."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sub = await subscribe_after_initial_states(client, entities)

        require_entity(entities, "test_valve", ValveInfo)

        press = partial(
            press_and_wait, client, entities, sub.waiter, state_type=ValveState
        )

        # valve.control: position only
        state = await press("set_position")
        assert state.position == pytest.approx(0.5, abs=0.01)

        # valve.control: state alias for position 1.0
        state = await press("open_state")
        assert state.position == pytest.approx(1.0, abs=0.01)

        # valve.control: lambda position (test_position global = 0.42)
        state = await press("lambda_position")
        assert state.position == pytest.approx(0.42, abs=0.01)

        # valve.control: stop only — template valve's stop_action publishes
        # current_operation: IDLE.
        state = await press("stop_valve")
        assert state.current_operation == ValveOperation.IDLE
