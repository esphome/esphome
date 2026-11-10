"""Tests for the state subscription helpers in tests/integration/state_utils.py."""

from __future__ import annotations

import asyncio
from collections.abc import Callable

from aioesphomeapi import ButtonInfo, EntityState, SensorInfo, SensorState, SwitchState
import pytest

from tests.integration.state_utils import (
    StateSubscription,
    StateWaiter,
    press_and_wait,
    subscribe_after_initial_states,
)

ENTITIES = [
    SensorInfo(object_id="temp", key=1),
    SensorInfo(object_id="humidity", key=2),
    ButtonInfo(object_id="set_temp", key=3),
]


class FakeClient:
    """Records the state callback and button presses."""

    def __init__(self) -> None:
        self.callback: Callable[[EntityState], None] | None = None
        self.pressed: list[int] = []

    def subscribe_states(self, callback: Callable[[EntityState], None]) -> None:
        self.callback = callback

    def button_command(self, key: int) -> None:
        self.pressed.append(key)
        self.callback(SensorState(key=1, state=42.0))


async def _subscribe(client: FakeClient, **kwargs: object) -> StateSubscription:
    task = asyncio.create_task(
        subscribe_after_initial_states(client, ENTITIES, **kwargs)
    )
    await asyncio.sleep(0)
    client.callback(SensorState(key=1, state=1.0))
    client.callback(SensorState(key=2, state=2.0))
    return await task


@pytest.mark.asyncio
async def test_subscribe_swallows_initial_states_and_forwards_later_ones() -> None:
    client = FakeClient()
    received: list[EntityState] = []
    sub = await _subscribe(client, on_state=received.append)
    assert sub.initial_states[1].state == 1.0
    assert sub.initial_states[2].state == 2.0
    assert received == []

    next_state = sub.waiter.next_state(key=2, state_type=SensorState)
    client.callback(SensorState(key=1, state=3.0))
    client.callback(SensorState(key=2, state=4.0))
    assert (await next_state).state == 4.0
    assert [s.state for s in received] == [3.0, 4.0]


@pytest.mark.asyncio
async def test_subscribe_timeout_lists_missing_entities() -> None:
    client = FakeClient()
    task = asyncio.create_task(
        subscribe_after_initial_states(client, ENTITIES, timeout=0.01)
    )
    await asyncio.sleep(0)
    client.callback(SensorState(key=1, state=1.0))
    with pytest.raises(pytest.fail.Exception, match=r"still_waiting[\s\S]*humidity"):
        await task


@pytest.mark.asyncio
async def test_next_state_filters_by_type_and_times_out() -> None:
    waiter = StateWaiter()
    next_switch = waiter.next_state(state_type=SwitchState)
    waiter.on_state(SensorState(key=1, state=1.0))
    waiter.on_state(SwitchState(key=5, state=True))
    assert (await next_switch).key == 5
    with pytest.raises(TimeoutError, match="SwitchState key=9"):
        await waiter.next_state(key=9, state_type=SwitchState, timeout=0.01)


@pytest.mark.asyncio
async def test_press_and_wait_presses_button_and_returns_state() -> None:
    client = FakeClient()
    sub = await _subscribe(client)
    state = await press_and_wait(client, ENTITIES, sub.waiter, "set_temp", SensorState)
    assert client.pressed == [3]
    assert state.state == 42.0


@pytest.mark.asyncio
async def test_press_and_wait_waits_for_the_given_key() -> None:
    client = FakeClient()
    sub = await _subscribe(client)
    pending = asyncio.ensure_future(
        press_and_wait(client, ENTITIES, sub.waiter, "set_temp", SensorState, key=2)
    )
    await asyncio.sleep(0)
    assert not pending.done()  # the press published key 1
    client.callback(SensorState(key=2, state=55.0))
    assert (await pending).state == 55.0
