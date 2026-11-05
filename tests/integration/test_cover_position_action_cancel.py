"""Integration test for how commands cancel a running position action on a template cover."""

from __future__ import annotations

import asyncio
from typing import Any

from aioesphomeapi import CoverInfo, CoverState, EntityState
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


def _seen(positions: list[float], value: float) -> bool:
    return any(p == pytest.approx(value, abs=0.01) for p in positions)


@pytest.mark.asyncio
async def test_cover_position_action_cancel(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that stop, toggle, open, close and a new position cancel a running position action."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        positions: list[float] = []

        def on_state(state: EntityState) -> None:
            if isinstance(state, CoverState):
                positions.append(state.position)

        entities, _ = await client.list_entities_services()
        await subscribe_after_initial_states(client, entities, on_state=on_state)

        cover = require_entity(entities, "test_cover", CoverInfo)

        # position_action publishes its target 1s after the command. The second
        # command comes 0.2s after a move to 0.5, so 0.5 shows up within the
        # 1.5s wait only if the position action for 0.5 was not cancelled.
        async def move_to_half_then(**command: Any) -> None:
            positions.clear()
            client.cover_command(key=cover.key, position=0.5)
            await asyncio.sleep(0.2)
            client.cover_command(key=cover.key, **command)
            await asyncio.sleep(1.5)
            assert not _seen(positions, 0.5)

        await move_to_half_then(stop=True)

        await move_to_half_then(toggle=True)

        # open_action publishes 1.0
        await move_to_half_then(position=1.0)
        assert positions[-1] == pytest.approx(1.0, abs=0.01)

        # close_action publishes 0.0
        await move_to_half_then(position=0.0)
        assert positions[-1] == pytest.approx(0.0, abs=0.01)

        # the position action for 0.3 runs to its end
        await move_to_half_then(position=0.3)
        assert positions[-1] == pytest.approx(0.3, abs=0.01)
