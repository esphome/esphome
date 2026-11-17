"""Integration test for the LD2450 target xy text sensor, with and without a throttle."""

from __future__ import annotations

import asyncio
from itertools import groupby

from aioesphomeapi import ButtonInfo, EntityState, TextSensorInfo, TextSensorState
import pytest

from .state_utils import require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction

# The positions of each target in the fixture, one per radar frame
TARGET_1 = [f"{-600 + 100 * i},{1000 + 50 * i}" for i in range(12)]
TARGET_2 = [f"{200 + 80 * i},{2000 + 30 * i}" for i in range(12)]
# Target 3 moves for 3 frames (300 ms) and leaves, with a 500 ms throttle
TARGET_3 = [f"{-300 + 50 * i},{3000 + 50 * i}" for i in range(3)]
# Frames are 100 ms apart: "NA" must come well before the 500 ms throttle ends
TARGET_3_LEAVES_WITHIN_S = 0.45


def _distinct(states: list[str]) -> list[str]:
    """Drop the repeat that the timeout filter sends after a pause."""
    return [state for state, _ in groupby(states)]


@pytest.mark.asyncio
async def test_uart_mock_ld2450_target_xy(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test the target xy text sensor with and without a throttle."""
    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        target_1 = require_entity(entities, "target_1_xy", TextSensorInfo)
        target_2 = require_entity(entities, "target_2_xy", TextSensorInfo)
        target_3 = require_entity(entities, "target_3_xy", TextSensorInfo)
        history: dict[int, list[str]] = {
            target.key: [] for target in (target_1, target_2, target_3)
        }
        target_3_times: list[float] = []
        loop = asyncio.get_running_loop()
        both_left = loop.create_future()

        def on_state(state: EntityState) -> None:
            if not isinstance(state, TextSensorState) or state.key not in history:
                return
            history[state.key].append(state.state)
            if state.key == target_3.key:
                target_3_times.append(loop.time())
            # Target 3 left long before, so only targets 1 and 2 end the scenario
            left = all(
                history[key] and history[key][-1] == "NA"
                for key in (target_1.key, target_2.key)
            )
            if left and not both_left.done():
                both_left.set_result(None)

        await subscribe_after_initial_states(client, entities, on_state=on_state)

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)
        await asyncio.wait_for(both_left, timeout=10.0)

        # Throttled: the first position at once, then a few, in order, and the final one before the target leaves
        throttled = _distinct(history[target_1.key])
        assert throttled[-1] == "NA"
        positions = throttled[:-1]
        assert positions[0] == TARGET_1[0]
        assert positions[-1] == TARGET_1[-1], (
            "the final position was held back for good"
        )
        indexes = [TARGET_1.index(position) for position in positions]
        assert indexes == sorted(set(indexes)), f"out of order or repeated: {positions}"
        assert 3 <= len(positions) <= 6, f"expected a few updates, got {positions}"

        # Not throttled: every change is published
        assert history[target_2.key] == [*TARGET_2, "NA"]

        # "NA" bypasses the throttle and drops the held position: nothing follows it
        assert _distinct(history[target_3.key]) == [TARGET_3[0], "NA"]
        assert target_3_times[1] - target_3_times[0] < TARGET_3_LEAVES_WITHIN_S
