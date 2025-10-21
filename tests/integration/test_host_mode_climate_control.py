"""Integration test for Host mode with climate."""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import ClimateInfo, ClimateMode, EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_climate_control(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test climate mode control."""
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        states: dict[int, EntityState] = {}
        climate_future: asyncio.Future[EntityState] = loop.create_future()

        # Get entity list
        entities = await client.list_entities_services()
        climate_infos = [e for e in entities[0] if isinstance(e, ClimateInfo)]
        assert len(climate_infos) >= 1, "Expected at least 1 climate entity"

        test_climate = next(
            (c for c in climate_infos if c.name == "Dual-mode Thermostat"), None
        )
        assert test_climate is not None, (
            "Dual-mode Thermostat thermostat climate not found"
        )

        # Adjust setpoints
        client.climate_command(
            test_climate.key,
            mode=ClimateMode.HEAT,
            target_temperature_low=21.5,
            target_temperature_high=26.5,
        )

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
        assert climate_state.mode == ClimateMode.HEAT
        assert climate_state.target_temperature_low == 21.5
        assert climate_state.target_temperature_high == 26.5
