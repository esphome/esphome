"""Integration test for inline ContinuationAction in If/While/RepeatAction.

Verifies that if/then, if/then/else, while, and repeat actions all execute
correctly with the inline-member ContinuationAction storage (replacing the
previous heap allocation).
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, EntityState, TextSensorInfo, TextSensorState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_inline_continuation_actions(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test if/while/repeat all execute correctly with inline continuations."""
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        text_state_future: asyncio.Future[TextSensorState] | None = None

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, TextSensorState)
                and text_state_future is not None
                and not text_state_future.done()
            ):
                text_state_future.set_result(state)

        async def wait_for_text_state(timeout: float = 5.0) -> TextSensorState:
            nonlocal text_state_future
            text_state_future = loop.create_future()
            try:
                return await asyncio.wait_for(text_state_future, timeout)
            finally:
                text_state_future = None

        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        # Verify the counter text sensor exists (used implicitly by get_counter)
        require_entity(entities, "counter_value", TextSensorInfo)

        async def press(name: str) -> None:
            btn = require_entity(entities, name.lower().replace(" ", "_"), ButtonInfo)
            client.button_command(btn.key)
            # Brief settle delay so the action chain runs to completion
            await asyncio.sleep(0.05)

        async def get_counter() -> int:
            """Force the template text sensor to update and read the value."""
            update_btn = require_entity(entities, "update_counter", ButtonInfo)
            client.button_command(update_btn.key)
            state = await wait_for_text_state()
            return int(state.state)

        # Test 1: if/then with HasElse=false — counter starts 0, enabled=true → +1
        await press("Reset")
        await press("If Then")
        assert await get_counter() == 1

        # Test 2: if/then/else (HasElse=true) with enabled=true → +10 (then branch)
        await press("Reset")
        await press("If Then Else")
        assert await get_counter() == 10

        # Test 3: while loop — counter=1 (1%5!=0) → loops to 5, exits
        await press("Reset")
        await press("If Then")  # counter=1
        await press("While")
        assert await get_counter() == 5

        # Test 4: repeat 3x +7 = 21
        await press("Reset")
        await press("Repeat")
        assert await get_counter() == 21

        # Test 5: if/then/else with enabled=false → +100 (else branch)
        await press("Reset")
        await press("Disable")
        await press("If Then Else")
        assert await get_counter() == 100
