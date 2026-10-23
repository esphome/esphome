"""Integration test for valve ControlAction.

Tests that valve.control automation actions work correctly across multiple
field combinations and the lambda path.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, EntityState, ValveInfo, ValveOperation, ValveState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_valve_control_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test valve ControlAction with constants and a lambda."""
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        valve_state_future: asyncio.Future[ValveState] | None = None

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, ValveState)
                and valve_state_future is not None
                and not valve_state_future.done()
            ):
                valve_state_future.set_result(state)

        async def wait_for_valve_state(timeout: float = 5.0) -> ValveState:
            nonlocal valve_state_future
            valve_state_future = loop.create_future()
            try:
                return await asyncio.wait_for(valve_state_future, timeout)
            finally:
                valve_state_future = None

        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        require_entity(entities, "test_valve", ValveInfo)

        async def press_and_wait(name: str) -> ValveState:
            btn = require_entity(entities, name.lower().replace(" ", "_"), ButtonInfo)
            client.button_command(btn.key)
            return await wait_for_valve_state()

        # valve.control: position only
        state = await press_and_wait("Set Position")
        assert state.position == pytest.approx(0.5, abs=0.01)

        # valve.control: state alias for position 1.0
        state = await press_and_wait("Open State")
        assert state.position == pytest.approx(1.0, abs=0.01)

        # valve.control: lambda position (test_position global = 0.42)
        state = await press_and_wait("Lambda Position")
        assert state.position == pytest.approx(0.42, abs=0.01)

        # valve.control: stop only — template valve's stop_action publishes
        # current_operation: IDLE.
        state = await press_and_wait("Stop Valve")
        assert state.current_operation == ValveOperation.IDLE
