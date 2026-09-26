"""Integration test verifying light effect restore on/off cycles.

Tests that when resume_effect is enabled, the previously active effect
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
async def test_light_resume_effect(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """When resume_effect is enabled, effect persists after off/on cycle."""
    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        light_resume = next(e for e in entities if e.object_id == "test_light_resume")
        light_no_resume = next(
            e for e in entities if e.object_id == "test_light_no_resume"
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
        state = await send_and_wait(light_resume, state=True, effect="Pulse Effect")
        assert state.state is True
        assert state.effect == "Pulse Effect"

        # Test 2: Turn off
        state = await send_and_wait(light_resume, state=False)
        assert state.state is False

        # Test 3: Turn on without specifying effect — should restore Pulse Effect
        state = await send_and_wait(light_resume, state=True)
        assert state.state is True
        assert state.effect == "Pulse Effect", (
            "Effect should be restored when turning on without explicit effect parameter"
        )

        # Test 4: Switch to Strobe effect
        state = await send_and_wait(light_resume, effect="Strobe Effect")
        assert state.effect == "Strobe Effect"

        # Test 5: Turn off
        state = await send_and_wait(light_resume, state=False)
        assert state.state is False

        # Test 6: Turn on without effect — should restore Strobe Effect
        state = await send_and_wait(light_resume, state=True)
        assert state.state is True
        assert state.effect == "Strobe Effect", (
            "Effect should be restored to Strobe after turning on"
        )

        # Test 7: Explicitly set effect to None while turning on — should not restore
        state = await send_and_wait(light_resume, state=False)
        assert state.state is False

        state = await send_and_wait(light_resume, state=True, effect="None")
        assert state.state is True
        assert state.effect == "None", (
            "Explicit effect=None should override restoration"
        )

        # Test 8: Turn off, then on with explicit effect — should use explicit effect
        state = await send_and_wait(light_resume, state=False)
        state = await send_and_wait(light_resume, state=True, effect="Pulse Effect")
        assert state.effect == "Pulse Effect"

        # Test 9: a turn-on that asks for something specific does not restore, and the
        # effect it replaced must not come back on a later plain off/on
        state = await send_and_wait(light_resume, state=False)
        assert state.state is False
        state = await send_and_wait(light_resume, state=True, brightness=0.5)
        assert state.effect == "None", "A turn-on with brightness should not restore"
        state = await send_and_wait(light_resume, state=False)
        state = await send_and_wait(light_resume, state=True)
        assert state.effect == "None", (
            "An effect dropped on an earlier cycle must not return"
        )

        # Test 10: a plain turn-on sent to a light that is already on never starts the
        # remembered effect
        state = await send_and_wait(light_resume, state=True, effect="Pulse Effect")
        state = await send_and_wait(light_resume, state=False)
        state = await send_and_wait(light_resume, state=True, brightness=0.5)
        assert state.effect == "None"
        state = await send_and_wait(light_resume, state=True)
        assert state.effect == "None", (
            "A lit light must not pick up the remembered effect"
        )

        # Test 11: Turn on effect, then off, then on without effect — should not restore Pulse Effect
        state = await send_and_wait(light_no_resume, state=True, effect="Pulse Effect")
        assert state.state is True
        assert state.effect == "Pulse Effect"

        state = await send_and_wait(light_no_resume, state=False)
        assert state.state is False

        state = await send_and_wait(light_no_resume, state=True)
        assert state.state is True
        assert state.effect == "None", (
            "Effect should NOT be restored when resume_effect=false"
        )
