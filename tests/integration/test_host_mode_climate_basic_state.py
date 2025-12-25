"""Integration test for Host mode with climate."""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import ClimateAction, ClimateMode, ClimatePreset, EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_climate_basic_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test basic climate state reporting."""
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        states: dict[int, EntityState] = {}
        climate_future: asyncio.Future[EntityState] = loop.create_future()

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            if (
                isinstance(state, aioesphomeapi.ClimateState)
                and not climate_future.done()
            ):
                climate_future.set_result(state)

        client.subscribe_states(on_state)

        try:
            climate_state = await asyncio.wait_for(climate_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Climate state not received within 5 seconds")

        assert isinstance(climate_state, aioesphomeapi.ClimateState)
        assert climate_state.mode == ClimateMode.OFF
        assert climate_state.action == ClimateAction.OFF
        assert climate_state.current_temperature == 22.0
        assert climate_state.target_temperature_low == 18.0
        assert climate_state.target_temperature_high == 24.0
        assert climate_state.preset == ClimatePreset.HOME
        assert climate_state.current_humidity == 42.0
        assert climate_state.target_humidity == 20.0
