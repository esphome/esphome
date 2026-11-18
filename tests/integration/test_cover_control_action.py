"""Integration test for the cover.control and cover.template.publish actions.

Tests that cover.control and cover.template.publish automation actions
work correctly with the single stateless apply lambda/function pointer
implementation. Exercises multiple field combinations and the lambda path.
"""

from __future__ import annotations

from functools import partial

from aioesphomeapi import CoverInfo, CoverState
import pytest

from .state_utils import press_and_wait, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_cover_control_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test cover.control and cover.template.publish with constants and lambdas."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sub = await subscribe_after_initial_states(client, entities)

        require_entity(entities, "test_cover", CoverInfo)

        press = partial(
            press_and_wait, client, entities, sub.waiter, state_type=CoverState
        )

        # cover.control: position only
        state = await press("set_position")
        assert state.position == pytest.approx(0.5, abs=0.01)

        # cover.control: tilt only
        state = await press("set_tilt")
        assert state.tilt == pytest.approx(0.75, abs=0.01)

        # cover.control: position + tilt
        state = await press("set_pos_tilt")
        assert state.position == pytest.approx(0.25, abs=0.01)
        assert state.tilt == pytest.approx(0.30, abs=0.01)

        # cover.control: state alias for position 1.0
        state = await press("open_state")
        assert state.position == pytest.approx(1.0, abs=0.01)

        # cover.control: lambda position (test_position global = 0.42)
        state = await press("lambda_position")
        assert state.position == pytest.approx(0.42, abs=0.01)

        # cover.template.publish: position only
        state = await press("publish_pos")
        assert state.position == pytest.approx(0.6, abs=0.01)

        # cover.template.publish: current_operation only
        state = await press("publish_op")
        # CoverOperation.OPENING == 1
        assert state.current_operation == 1

        # cover.control: stop only — template cover's stop_action publishes
        # current_operation: IDLE.
        state = await press("stop_cover")
        # CoverOperation.IDLE == 0
        assert state.current_operation == 0

        # stop_tilt_action publishes tilt 0.4
        state = await press("stop_tilt")
        assert state.tilt == pytest.approx(0.4, abs=0.01)

        state = await press("set_tilt")
        assert state.tilt == pytest.approx(0.75, abs=0.01)

        state = await press("control_stop_tilt")
        assert state.tilt == pytest.approx(0.4, abs=0.01)
