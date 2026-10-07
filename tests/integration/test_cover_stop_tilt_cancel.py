"""Integration test for cancelling a pending template cover stop_tilt_action."""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, CoverInfo, CoverState, EntityState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_cover_stop_tilt_cancel(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that a tilt command cancels a pending stop_tilt_action."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        tilts: list[float] = []

        def on_state(state: EntityState) -> None:
            if isinstance(state, CoverState):
                tilts.append(state.tilt)

        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        cover = require_entity(entities, "test_cover", CoverInfo)
        button = require_entity(entities, "stop_tilt", ButtonInfo)

        client.button_command(button.key)
        await asyncio.sleep(0.2)
        client.cover_command(key=cover.key, tilt=0.75)
        await asyncio.sleep(2.0)

        assert tilts
        assert tilts[-1] == pytest.approx(0.75, abs=0.01)
        assert not any(t == pytest.approx(0.4, abs=0.01) for t in tilts)
