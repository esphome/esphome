"""Integration test for Host mode with climate."""

from __future__ import annotations

import aioesphomeapi
from aioesphomeapi import ClimateInfo, ClimateMode
import pytest

from .state_utils import require_only_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_climate_control(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test climate mode control."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get entities and set up state synchronization
        entities, services = await client.list_entities_services()
        test_climate = require_only_entity(entities, ClimateInfo)
        assert test_climate.name == "Dual-mode Thermostat"

        # Wait for all initial states to be broadcast
        sub = await subscribe_after_initial_states(client, entities)

        # Adjust setpoints
        climate_seen = sub.waiter.expect(
            lambda s: (
                isinstance(s, aioesphomeapi.ClimateState)
                and s.mode == ClimateMode.HEAT
                and s.target_temperature_low == 21.5
                and s.target_temperature_high == 26.5
            ),
            label="Climate state",
        )
        client.climate_command(
            test_climate.key,
            mode=ClimateMode.HEAT,
            target_temperature_low=21.5,
            target_temperature_high=26.5,
        )

        climate_state = await climate_seen

        assert isinstance(climate_state, aioesphomeapi.ClimateState)
        assert climate_state.mode == ClimateMode.HEAT
        assert climate_state.target_temperature_low == 21.5
        assert climate_state.target_temperature_high == 26.5
