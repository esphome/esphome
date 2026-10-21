"""Integration test for Host mode with climate."""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import ClimateInfo, ClimateMode, EntityState
import pytest

from .state_utils import InitialStateHelper, require_only_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


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

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            if (
                isinstance(state, aioesphomeapi.ClimateState)
                and state.mode == ClimateMode.HEAT
                and state.target_temperature_low == 21.5
                and state.target_temperature_high == 26.5
                and not climate_future.done()
            ):
                climate_future.set_result(state)

        # Get entities and set up state synchronization
        entities, services = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        test_climate = require_only_entity(entities, ClimateInfo)
        assert test_climate.name == "Dual-mode Thermostat"

        # Subscribe with the wrapper that filters initial states
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for all initial states to be broadcast
        await initial_state_helper.wait_for_initial_states()

        # Adjust setpoints
        client.climate_command(
            test_climate.key,
            mode=ClimateMode.HEAT,
            target_temperature_low=21.5,
            target_temperature_high=26.5,
        )

        climate_state = await wait_or_fail(
            climate_future, 5.0, "Climate state not received within 5 seconds"
        )

        assert isinstance(climate_state, aioesphomeapi.ClimateState)
        assert climate_state.mode == ClimateMode.HEAT
        assert climate_state.target_temperature_low == 21.5
        assert climate_state.target_temperature_high == 26.5
