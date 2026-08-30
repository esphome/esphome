"""Integration test for cover ControlAction and CoverPublishAction.

Tests that cover.control and cover.template.publish automation actions
work correctly with the single stateless apply lambda/function pointer
implementation. Exercises multiple field combinations and the lambda path.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, CoverInfo, CoverState, EntityState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_cover_control_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test cover ControlAction/CoverPublishAction with constants and lambdas."""
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        cover_state_future: asyncio.Future[CoverState] | None = None

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, CoverState)
                and cover_state_future is not None
                and not cover_state_future.done()
            ):
                cover_state_future.set_result(state)

        async def wait_for_cover_state(timeout: float = 5.0) -> CoverState:
            nonlocal cover_state_future
            cover_state_future = loop.create_future()
            try:
                return await asyncio.wait_for(cover_state_future, timeout)
            finally:
                cover_state_future = None

        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        require_entity(entities, "test_cover", CoverInfo)

        async def press_and_wait(name: str) -> CoverState:
            btn = require_entity(entities, name.lower().replace(" ", "_"), ButtonInfo)
            client.button_command(btn.key)
            return await wait_for_cover_state()

        # cover.control: position only
        state = await press_and_wait("Set Position")
        assert state.position == pytest.approx(0.5, abs=0.01)

        # cover.control: tilt only
        state = await press_and_wait("Set Tilt")
        assert state.tilt == pytest.approx(0.75, abs=0.01)

        # cover.control: position + tilt
        state = await press_and_wait("Set Pos Tilt")
        assert state.position == pytest.approx(0.25, abs=0.01)
        assert state.tilt == pytest.approx(0.30, abs=0.01)

        # cover.control: state alias for position 1.0
        state = await press_and_wait("Open State")
        assert state.position == pytest.approx(1.0, abs=0.01)

        # cover.control: lambda position (test_position global = 0.42)
        state = await press_and_wait("Lambda Position")
        assert state.position == pytest.approx(0.42, abs=0.01)

        # cover.template.publish: position only
        state = await press_and_wait("Publish Pos")
        assert state.position == pytest.approx(0.6, abs=0.01)

        # cover.template.publish: current_operation only
        state = await press_and_wait("Publish Op")
        # CoverOperation.OPENING == 1
        assert state.current_operation == 1

        # cover.control: stop only — template cover's stop_action publishes
        # current_operation: IDLE.
        state = await press_and_wait("Stop Cover")
        # CoverOperation.IDLE == 0
        assert state.current_operation == 0
