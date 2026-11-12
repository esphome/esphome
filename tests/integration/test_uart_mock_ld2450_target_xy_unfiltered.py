"""Integration test for the LD2450 xy_unfiltered button, which bypasses the xy text sensor filters for 10 s."""

from __future__ import annotations

import asyncio
from itertools import groupby

from aioesphomeapi import ButtonInfo, EntityState, TextSensorInfo, TextSensorState
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction

# The positions of target 1 in the fixture, one per radar frame (100 ms apart)
POSITIONS = [f"{-600 + 100 * i},{1000 + 50 * i}" for i in range(14)]
# The positions of target 2, which is seen in the first 2 frames
TARGET_2_POSITIONS = [f"{200 + 80 * i},{2000 + 30 * i}" for i in range(2)]
# The frames of each group, as the fixture sends them
GROUP_A_FRAMES = POSITIONS[0:4]  # at 0.1 s, in the first window
GROUP_B_FRAMES = POSITIONS[4:8]  # at 11 s: only in the window the second press extended
GROUP_C_FRAMES = POSITIONS[8:12]  # at 18.5 s: the window is over
# at 21 s: the filters hold back the second one
GROUP_D_FRAMES = POSITIONS[12:14]
# The first press is just before the scenario starts: its window ends at about 10 s. The second press extends it
# to about 16 s.
SECOND_PRESS_AFTER_S = 6.0


def _distinct(states: list[str]) -> list[str]:
    """Drop the repeats that the filters send when the window ends and after a pause."""
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
        target_2 = require_entity(entities, "target_2_xy", TextSensorInfo)
        unfiltered_btn = require_entity(entities, "xy_unfiltered", ButtonInfo)
        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        history: list[str] = []
        history_2: list[str] = []
        loop = asyncio.get_running_loop()
        group_a_published = loop.create_future()
        first_of_d_published = loop.create_future()
        second_of_d_published = loop.create_future()
        target_left = loop.create_future()

        def on_state(state: EntityState) -> None:
            if isinstance(state, TextSensorState) and state.key == target_2.key:
                history_2.append(state.state)
            if not isinstance(state, TextSensorState) or state.key != target_1.key:
                return
            history.append(state.state)
            if len(history) == len(GROUP_A_FRAMES) and not group_a_published.done():
                group_a_published.set_result(None)
            if state.state == GROUP_D_FRAMES[0] and not first_of_d_published.done():
                first_of_d_published.set_result(None)
            if state.state == GROUP_D_FRAMES[1] and not second_of_d_published.done():
                second_of_d_published.set_result(None)
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

        await asyncio.wait_for(first_of_d_published, timeout=20.0)
        before_c = len(GROUP_A_FRAMES) + len(GROUP_B_FRAMES)
        # Group B came after the first window: only the second press can have kept the filters off
        assert history[:before_c] == [*GROUP_A_FRAMES, *GROUP_B_FRAMES]
        # When the window ends (at about 16 s), the filters run the current position again, which repeats it.
        # Group C: they work as at boot. The first position passes at once, the others are held back, and the
        # last one is sent when updates pause.
        assert _distinct(history[before_c - 1 :]) == [
            GROUP_B_FRAMES[-1],
            GROUP_C_FRAMES[0],
            GROUP_C_FRAMES[-1],
            GROUP_D_FRAMES[0],
        ]

        # Group D: the filters hold the second position back, and the target stops there. Pressing the button
        # sends it at once, long before the filters would have.
        await asyncio.sleep(0.3)
        assert history[-1] == GROUP_D_FRAMES[0]
        client.button_command(unfiltered_btn.key)
        await asyncio.wait_for(second_of_d_published, timeout=0.6)

        await asyncio.wait_for(target_left, timeout=10.0)
        assert _distinct(history[before_c - 1 :]) == [
            GROUP_B_FRAMES[-1],
            GROUP_C_FRAMES[0],
            GROUP_C_FRAMES[-1],
            *GROUP_D_FRAMES,
            "NA",
        ]

        # Target 2 leaves in the first window, so it publishes "NA" unfiltered. When the first press's window
        # ends (here, the extended one), the filters are back and run the current value, so the map of "NA"
        # applies. The press in group D then sends the raw "NA" again.
        assert history_2 == [*TARGET_2_POSITIONS, "NA", "Away", "NA"]
