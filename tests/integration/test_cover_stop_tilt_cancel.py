"""Integration test for how stop tilt and tilt commands cancel each other on a template cover."""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, CoverInfo, CoverState, EntityState
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


def _seen(tilts: list[float], value: float) -> bool:
    return any(t == pytest.approx(value, abs=0.01) for t in tilts)


@pytest.mark.asyncio
async def test_cover_stop_tilt_cancel(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that stop tilt and tilt commands cancel each other's pending actions."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        tilts: list[float] = []

        def on_state(state: EntityState) -> None:
            if isinstance(state, CoverState):
                tilts.append(state.tilt)

        entities, _ = await client.list_entities_services()
        await subscribe_after_initial_states(client, entities, on_state=on_state)

        cover = require_entity(entities, "test_cover", CoverInfo)
        button = require_entity(entities, "stop_tilt", ButtonInfo)

        # tilt_action publishes 0.9 after 1s, stop_tilt_action publishes 0.4 after 1s
        tilts.clear()
        client.cover_command(key=cover.key, tilt=0.75)
        await asyncio.sleep(0.2)
        client.button_command(button.key)
        await asyncio.sleep(2.0)
        assert not _seen(tilts, 0.9)
        assert tilts[-1] == pytest.approx(0.4, abs=0.01)

        tilts.clear()
        client.button_command(button.key)
        await asyncio.sleep(0.2)
        client.cover_command(key=cover.key, tilt=0.6)
        await asyncio.sleep(2.0)
        after_tilt = tilts[[round(t, 2) for t in tilts].index(0.6) :]
        assert not _seen(after_tilt, 0.4)
        assert tilts[-1] == pytest.approx(0.9, abs=0.01)
