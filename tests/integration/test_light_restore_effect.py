"""Integration test verifying light effect restore on/off cycles.

Tests that when restore_effect is enabled, the previously active effect
is restored when the light is turned back on without explicit effect/color
parameters.
"""

from __future__ import annotations

import asyncio
from typing import Any

from aioesphomeapi import EntityState, LightState
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_restore_effect(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """When restore_effect is enabled, effect persists after off/on cycle."""
    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        light_restore = next(e for e in entities if e.object_id == "test_light_restore")
        light_no_restore = next(
            e for e in entities if e.object_id == "test_light_no_restore"
        )

        state_futures: dict[int, asyncio.Future[LightState]] = {}

        def on_state(state: EntityState) -> None:
            if isinstance(state, LightState) and state.key in state_futures:
                future = state_futures[state.key]
                if not future.done():
                    future.set_result(state)

        # Drain initial state burst
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        async def send_and_wait(
            light, timeout: float = 5.0, **kwargs: Any
        ) -> LightState:
            """Send a light command and wait for the matching state response."""
            state_futures[light.key] = asyncio.get_running_loop().create_future()
            client.light_command(key=light.key, **kwargs)
            return await asyncio.wait_for(state_futures[light.key], timeout=timeout)

        # Test 1: Start with Pulse effect
        state = await send_and_wait(light_restore, state=True, effect="Pulse Effect")
        assert state.state is True
        assert state.effect == "Pulse Effect"

        # Test 2: Turn off
        state = await send_and_wait(light_restore, state=False)
        assert state.state is False

        # Test 3: Turn on without specifying effect — should restore Pulse Effect
        state = await send_and_wait(light_restore, state=True)
        assert state.state is True
        assert state.effect == "Pulse Effect", (
            "Effect should be restored when turning on without explicit effect parameter"
        )

        # Test 4: Switch to Strobe effect
        state = await send_and_wait(light_restore, effect="Strobe Effect")
        assert state.effect == "Strobe Effect"

        # Test 5: Turn off
        state = await send_and_wait(light_restore, state=False)
        assert state.state is False

        # Test 6: Turn on without effect — should restore Strobe Effect
        state = await send_and_wait(light_restore, state=True)
        assert state.state is True
        assert state.effect == "Strobe Effect", (
            "Effect should be restored to Strobe after turning on"
        )

        # Test 7: Explicitly set effect to None while turning on — should not restore
        state = await send_and_wait(light_restore, state=False)
        assert state.state is False

        state = await send_and_wait(light_restore, state=True, effect="None")
        assert state.state is True
        assert state.effect == "None", (
            "Explicit effect=None should override restoration"
        )

        # Test 8: Turn off, then on with explicit effect — should use explicit effect
        state = await send_and_wait(light_restore, state=False)
        state = await send_and_wait(light_restore, state=True, effect="Pulse Effect")
        assert state.effect == "Pulse Effect"

        # Test 9: Turn on effect, then off, then on without effect — should not restore Pulse Effect
        state = await send_and_wait(light_no_restore, state=True, effect="Pulse Effect")
        assert state.state is True
        assert state.effect == "Pulse Effect"

        state = await send_and_wait(light_no_restore, state=False)
        assert state.state is False

        state = await send_and_wait(light_no_restore, state=True)
        assert state.state is True
        assert state.effect == "None", (
            "Effect should NOT be restored when restore_effect=false"
        )
