"""Integration test that every way a feedback cover starts moving wakes its idle loop."""

from __future__ import annotations

import asyncio
from collections.abc import Awaitable

from aioesphomeapi import CoverInfo, CoverOperation, CoverState, EntityState, SwitchInfo
import pytest

from .log_utils import LineWaiter
from .state_utils import StateWaiter, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


def _expect_idle(waiter: StateWaiter, cover: CoverInfo) -> Awaitable[EntityState]:
    return waiter.expect(
        lambda s: (
            isinstance(s, CoverState)
            and s.key == cover.key
            and s.current_operation == CoverOperation.IDLE
        ),
        label=f"{cover.name} IDLE",
    )


@pytest.mark.asyncio
async def test_feedback_cover_loop_wake(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that target position, max_duration and movement sensors still work with the loop idled."""
    lines = LineWaiter()
    max_stopped = lines.expect("Max duration reached")
    async with (
        run_compiled(yaml_config, line_callback=lines.callback),
        api_client_connected() as client,
    ):
        sensor_states: list[CoverState] = []
        entities, _ = await client.list_entities_services()
        time_cover = require_entity(entities, "time_cover", CoverInfo)
        max_cover = require_entity(entities, "max_duration_cover", CoverInfo)
        sensor_cover = require_entity(entities, "sensor_cover", CoverInfo)

        def on_state(state: EntityState) -> None:
            if isinstance(state, CoverState) and state.key == sensor_cover.key:
                sensor_states.append(state)

        sub = await subscribe_after_initial_states(client, entities, on_state=on_state)
        waiter = sub.waiter

        # Positions are restored from the previous run, so pick targets away from them
        start = sub.initial_states[time_cover.key].position
        target = 0.2 if start >= 0.5 else 0.8

        # Only loop() stops a time-only cover at its target
        done = _expect_idle(waiter, time_cover)
        client.cover_command(key=time_cover.key, position=target)
        state = await done
        assert state.position == pytest.approx(target, abs=0.05)

        # Stop mid move, then a new command must move it again and stop at the target
        stopped = _expect_idle(waiter, time_cover)
        client.cover_command(key=time_cover.key, position=start)
        await asyncio.sleep(0.3)
        client.cover_command(key=time_cover.key, stop=True)
        mid = (await stopped).position
        assert mid != pytest.approx(target, abs=0.05)
        done = _expect_idle(waiter, time_cover)
        client.cover_command(key=time_cover.key, position=target)
        state = await done
        assert state.position == pytest.approx(target, abs=0.05)

        # 1s max_duration against a 3s travel
        max_start = sub.initial_states[max_cover.key].position
        max_target = 0.0 if max_start >= 0.5 else 1.0
        done = _expect_idle(waiter, max_cover)
        client.cover_command(key=max_cover.key, position=max_target)
        state = await done
        await asyncio.wait_for(max_stopped, 1.0)
        assert abs(state.position - max_start) == pytest.approx(1 / 3, abs=0.1)

        # Moved by the movement sensor alone, no command
        sensor_start = sub.initial_states[sensor_cover.key].position
        switch_name = "external_close" if sensor_start >= 0.5 else "external_open"
        switch = require_entity(entities, switch_name, SwitchInfo)
        sensor_states.clear()
        client.switch_command(switch.key, True)
        await asyncio.sleep(1.2)
        settled = _expect_idle(waiter, sensor_cover)
        client.switch_command(switch.key, False)
        await settled
        moving = {
            round(s.position, 3)
            for s in sensor_states
            if s.current_operation != CoverOperation.IDLE
        }
        # One state from the sensor callback, the rest are loop() updates every 200ms
        assert len(moving) >= 4, sensor_states
