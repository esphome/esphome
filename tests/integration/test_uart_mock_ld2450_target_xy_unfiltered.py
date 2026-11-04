"""Integration test for the LD2450 xy_unfiltered button, which bypasses the xy text sensor filters for 10 s."""

from __future__ import annotations

import asyncio
from itertools import groupby

from aioesphomeapi import ButtonInfo, EntityState, TextSensorInfo, TextSensorState
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction

# The positions of target 1 in the fixture, one per radar frame (100 ms apart)
POSITIONS = [f"{-600 + 100 * i},{1000 + 50 * i}" for i in range(12)]
# The frames of each group, as the fixture sends them
GROUP_A_FRAMES = POSITIONS[0:4]  # at 0.1 s, in the first window
GROUP_B_FRAMES = POSITIONS[4:8]  # at 11 s: only in the window the second press extended
GROUP_C_FRAMES = POSITIONS[8:12]  # at 17 s: the window is over
# The first press is just before the scenario starts: its window ends at about 10 s. The second press extends it
# to about 16 s.
SECOND_PRESS_AFTER_S = 6.0


def _distinct(states: list[str]) -> list[str]:
    """Drop the repeat that the timeout filter sends after a pause."""
    return [state for state, _ in groupby(states)]


@pytest.mark.asyncio
async def test_uart_mock_ld2450_target_xy_unfiltered(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The button bypasses the filters for a while, pressing again extends it, and then the filters are back."""
    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        target_1 = require_entity(entities, "target_1_xy", TextSensorInfo)
        unfiltered_btn = require_entity(entities, "xy_unfiltered", ButtonInfo)
        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        history: list[str] = []
        loop = asyncio.get_running_loop()
        group_a_published = loop.create_future()
        target_left = loop.create_future()

        def on_state(state: EntityState) -> None:
            if not isinstance(state, TextSensorState) or state.key != target_1.key:
                return
            history.append(state.state)
            if len(history) == len(GROUP_A_FRAMES) and not group_a_published.done():
                group_a_published.set_result(None)
            if state.state == "NA" and not target_left.done():
                target_left.set_result(None)

        await subscribe_after_initial_states(client, entities, on_state=on_state)

        client.button_command(unfiltered_btn.key)
        client.button_command(start_btn.key)

        # Group A: every position, although the default filters allow one per second
        await asyncio.wait_for(group_a_published, timeout=5.0)
        assert history == GROUP_A_FRAMES

        await asyncio.sleep(SECOND_PRESS_AFTER_S)
        client.button_command(unfiltered_btn.key)

        await asyncio.wait_for(target_left, timeout=20.0)

        # Group B came after the first window: only the second press can have kept the filters off
        assert history[: len(GROUP_A_FRAMES) + len(GROUP_B_FRAMES)] == [
            *GROUP_A_FRAMES,
            *GROUP_B_FRAMES,
        ]
        # Group C: the filters are back, and work as at boot. The first position passes at once, the others are
        # held back, and "NA" passes at once and drops them.
        assert _distinct(history[len(GROUP_A_FRAMES) + len(GROUP_B_FRAMES) :]) == [
            GROUP_C_FRAMES[0],
            "NA",
        ]
