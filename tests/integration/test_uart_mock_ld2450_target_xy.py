"""Integration test for the LD2450 target xy text sensor with mock UART.

Tests:
1. Every published position is the x and y of one radar frame, as "x,y" text
2. With a throttle, positions are held back, in order, and the final position of a target that stops is published
3. Without a throttle, every change is published
4. A target leaving is published at once as "NA"
"""

from __future__ import annotations

import asyncio
import re

from aioesphomeapi import ButtonInfo, EntityState, TextSensorInfo, TextSensorState
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

# The positions of each target in the fixture, one per radar frame
TARGET_1 = [f"{-600 + 100 * i},{1000 + 50 * i}" for i in range(12)]
TARGET_2 = [f"{200 + 80 * i},{2000 + 30 * i}" for i in range(12)]


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
        history: dict[int, list[str]] = {target_1.key: [], target_2.key: []}
        both_left = asyncio.get_running_loop().create_future()

        def on_state(state: EntityState) -> None:
            if not isinstance(state, TextSensorState) or state.key not in history:
                return
            history[state.key].append(state.state)
            left = all(values and values[-1] == "NA" for values in history.values())
            if left and not both_left.done():
                both_left.set_result(None)

        helper = InitialStateHelper(entities)
        client.subscribe_states(helper.on_state_wrapper(on_state))
        await helper.wait_for_initial_states()

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)
        await asyncio.wait_for(both_left, timeout=10.0)

        for values in history.values():
            # Positions are "x,y" text, and a target leaving is "NA"
            assert all(re.fullmatch(r"-?\d+,-?\d+|NA", value) for value in values)

        # Throttled: the first position at once, then a few, in order, and the final one before the target leaves
        throttled = history[target_1.key]
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
