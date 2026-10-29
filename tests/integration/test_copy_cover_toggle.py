"""A toggle sent to a copy cover runs the toggle action of its source cover."""

from __future__ import annotations

from aioesphomeapi import CoverInfo, CoverState
import pytest

from .state_utils import press_and_wait, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_copy_cover_toggle(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sub = await subscribe_after_initial_states(client, entities)
        copy_cover = require_entity(entities, "copy_cover", CoverInfo)

        # Both covers start fully open (1.0); the source's toggle_action publishes 0.5
        state = await press_and_wait(
            client,
            entities,
            sub.waiter,
            "toggle_copy",
            CoverState,
            key=copy_cover.key,
        )
        assert state.position == pytest.approx(0.5, abs=0.01)
