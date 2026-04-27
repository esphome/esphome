"""Integration test for cover ControlAction and CoverPublishAction.

Tests that cover.control and cover.template.publish automation actions
work correctly with the per-instance bitmask field storage. Exercises
multiple field combinations to cover the bitmask variants.
"""

import asyncio
from typing import Any

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_cover_control_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test cover ControlAction/CoverPublishAction with constants and lambdas."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        state_futures: dict[int, asyncio.Future[Any]] = {}

        def on_state(state: Any) -> None:
            if state.key in state_futures and not state_futures[state.key].done():
                state_futures[state.key].set_result(state)

        client.subscribe_states(on_state)

        entities = await client.list_entities_services()
        cover = next(e for e in entities[0] if e.object_id == "test_cover")
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
            return await wait_for_state(cover.key)

        # Test 1: position only (mask 2)
        state = await press_and_wait("Set Position")
        assert state.position == pytest.approx(0.5, abs=0.01)

        # Test 2: tilt only (mask 4)
        state = await press_and_wait("Set Tilt")
        assert state.tilt == pytest.approx(0.75, abs=0.01)

        # Test 3: position + tilt (mask 6)
        state = await press_and_wait("Set Pos Tilt")
        assert state.position == pytest.approx(0.25, abs=0.01)
        assert state.tilt == pytest.approx(0.30, abs=0.01)

        # Test 4: stop (mask 1) — stop on a non-moving cover should not change state
        # We just verify the action runs without error; no state change expected.
        btn = buttons["Stop Cover"]
        client.button_command(btn.key)
        # Give the action a moment to execute
        await asyncio.sleep(0.1)

        # Test 5: state: OPEN (CONF_STATE alias for position 1.0)
        state = await press_and_wait("Open State")
        assert state.position == pytest.approx(1.0, abs=0.01)

        # Test 6: lambda position
        state = await press_and_wait("Lambda Position")
        assert state.position == pytest.approx(0.42, abs=0.01)

        # Test 7: cover.template.publish position only
        state = await press_and_wait("Publish Pos")
        assert state.position == pytest.approx(0.6, abs=0.01)

        # Test 8: cover.template.publish current_operation only
        state = await press_and_wait("Publish Op")
        # Operation 1 = OPENING
        assert state.current_operation == 1
