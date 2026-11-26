"""Integration test for light.move and light.stop."""

from __future__ import annotations

import asyncio
from functools import partial

from aioesphomeapi import LightInfo, LightState
import pytest

from .state_utils import press_and_wait, require_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_light_move_stop(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A move reports its target at once, a stop reports where the light got to."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        sub = await subscribe_after_initial_states(client, entities)
        mono = require_entity(entities, "mono_light", LightInfo).key
        cwww = require_entity(entities, "cwww_light", LightInfo).key

        press = partial(
            press_and_wait, client, entities, sub.waiter, state_type=LightState
        )

        state = await press("mono_setup", key=mono)
        assert state.brightness == pytest.approx(0.2, abs=0.01)

        # 20% to 100% takes 1.6 s at 2 s for the whole range
        state = await press("mono_up", key=mono)
        assert state.state is True
        assert state.brightness == pytest.approx(1.0)
        await asyncio.sleep(0.5)
        state = await press("mono_stop", key=mono)
        assert 0.25 < state.brightness < 0.95

        state = await press("mono_down", key=mono)
        assert state.brightness == pytest.approx(0.1)
        state = await press("mono_stop", key=mono)
        assert 0.1 < state.brightness < 0.95

        # From off, up turns the light on and moves to 100%; the first publish is that target, not an on-at-minimum
        state = await press("mono_off", key=mono)
        assert state.state is False
        state = await press("mono_up", key=mono)
        assert state.state is True
        assert state.brightness == pytest.approx(1.0)
        await asyncio.sleep(0.3)
        state = await press("mono_stop", key=mono)
        assert state.state is True
        assert 0.01 < state.brightness < 0.95

        state = await press("ct_setup", key=cwww)
        assert state.color_temperature == pytest.approx(300.0)
        # Up is warmer: more mireds
        state = await press("ct_up", key=cwww)
        assert state.color_temperature == pytest.approx(500.0)
        await asyncio.sleep(0.5)
        state = await press("ct_stop", key=cwww)
        assert 300.0 < state.color_temperature < 500.0
