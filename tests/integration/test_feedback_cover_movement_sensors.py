"""Integration test for a feedback cover driven by open and close movement sensors."""

from __future__ import annotations

import asyncio

from aioesphomeapi import CoverInfo, CoverOperation, CoverState, EntityState, SwitchInfo
import pytest

from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_feedback_cover_movement_sensors(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test max_duration, a self-stopping motor, a flickering sensor and reverses with no wait time,
    including one where the movement sensors overlap."""
    lines: list[str] = []

    def count(marker: str) -> int:
        return sum(marker in line for line in lines)

    async with (
        run_compiled(yaml_config, line_callback=lines.append),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        states: dict[int, EntityState] = {}
        client.subscribe_states(lambda state: states.__setitem__(state.key, state))
        reverse = require_entity(entities, "reverse_cover", CoverInfo)
        stuck = require_entity(entities, "stuck_cover", CoverInfo)
        opening = require_entity(entities, "reverse_opening", SwitchInfo)
        closing = require_entity(entities, "reverse_closing", SwitchInfo)
        limit = require_entity(entities, "limit_cover", CoverInfo)
        limit_opening = require_entity(entities, "limit_opening", SwitchInfo)
        flicker = require_entity(entities, "flicker_cover", CoverInfo)
        flicker_opening = require_entity(entities, "flicker_opening", SwitchInfo)
        dead = require_entity(entities, "dead_reverse_cover", CoverInfo)
        dead_opening = require_entity(entities, "dead_reverse_opening", SwitchInfo)
        overlap = require_entity(entities, "overlap_cover", CoverInfo)
        overlap_opening = require_entity(entities, "overlap_opening", SwitchInfo)
        overlap_closing = require_entity(entities, "overlap_closing", SwitchInfo)

        # The motor never reports movement, so max_duration has to stop it
        client.cover_command(key=stuck.key, position=1.0)
        await asyncio.sleep(1.5)
        assert count("STUCK_OPEN_ACTION") == 1
        assert count("STUCK_STOP_ACTION") == 1

        # The motor stops itself before the estimate reaches the target; max_duration must not fire later
        client.cover_command(key=limit.key, position=1.0)
        await asyncio.sleep(0.3)
        client.switch_command(key=limit_opening.key, state=True)
        await asyncio.sleep(0.3)
        client.switch_command(key=limit_opening.key, state=False)
        await asyncio.sleep(1.5)
        assert count("LIMIT_OPEN_ACTION") == 1
        assert count("LIMIT_STOP_ACTION") == 0
        limit_state = states[limit.key]
        assert isinstance(limit_state, CoverState)
        assert limit_state.current_operation == CoverOperation.IDLE

        # A sensor that drops out briefly mid-move must not lose the stop at the target
        client.cover_command(key=flicker.key, position=0.75)
        await asyncio.sleep(0.2)
        client.switch_command(key=flicker_opening.key, state=True)
        await asyncio.sleep(0.3)
        client.switch_command(key=flicker_opening.key, state=False)
        await asyncio.sleep(0.1)
        client.switch_command(key=flicker_opening.key, state=True)
        await asyncio.sleep(1.5)
        assert count("FLICKER_STOP_ACTION") == 1
        flicker_state = states[flicker.key]
        assert isinstance(flicker_state, CoverState)
        assert flicker_state.current_operation == CoverOperation.IDLE
        assert flicker_state.position == pytest.approx(0.75, abs=0.05)

        # The reversed motor never starts, so max_duration stops it once the old movement ends
        client.cover_command(key=dead.key, position=1.0)
        await asyncio.sleep(0.1)
        client.switch_command(key=dead_opening.key, state=True)
        await asyncio.sleep(0.1)
        client.cover_command(key=dead.key, position=0.0)
        await asyncio.sleep(0.1)
        client.switch_command(key=dead_opening.key, state=False)
        await asyncio.sleep(1.0)
        assert count("DEAD_CLOSE_ACTION") == 1
        assert count("DEAD_STOP_ACTION") == 2

        # The close sensor starts before the open sensor ends; max_duration must still stop the close
        client.cover_command(key=overlap.key, position=1.0)
        await asyncio.sleep(0.1)
        client.switch_command(key=overlap_opening.key, state=True)
        await asyncio.sleep(0.1)
        client.cover_command(key=overlap.key, position=0.0)
        await asyncio.sleep(0.1)
        client.switch_command(key=overlap_closing.key, state=True)
        await asyncio.sleep(0.1)
        client.switch_command(key=overlap_opening.key, state=False)
        await asyncio.sleep(0.2)
        overlap_state = states[overlap.key]
        assert isinstance(overlap_state, CoverState)
        assert overlap_state.current_operation == CoverOperation.IS_CLOSING
        await asyncio.sleep(1.2)
        assert count("OVERLAP_CLOSE_ACTION") == 1
        assert count("OVERLAP_STOP_ACTION") == 2

        # Reverse while the open sensor still reports movement for a while
        client.cover_command(key=reverse.key, position=1.0)
        await asyncio.sleep(0.3)
        client.switch_command(key=opening.key, state=True)
        await asyncio.sleep(0.5)
        client.cover_command(key=reverse.key, position=0.0)
        await asyncio.sleep(0.3)
        client.switch_command(key=opening.key, state=False)
        await asyncio.sleep(0.3)
        client.switch_command(key=closing.key, state=True)
        await asyncio.sleep(0.3)
        assert count("REVERSE_OPEN_ACTION") == 1
        assert count("REVERSE_STOP_ACTION") == 1
        assert count("REVERSE_CLOSE_ACTION") == 1
