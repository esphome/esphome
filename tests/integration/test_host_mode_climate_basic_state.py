"""Integration test for Host mode with climate."""

from __future__ import annotations

import aioesphomeapi
from aioesphomeapi import ClimateAction, ClimateInfo, ClimateMode, ClimatePreset
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_climate_basic_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test basic climate state reporting."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get entities and set up state synchronization
        entities, services = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) >= 1, "Expected at least 1 climate entity"

        # Subscribe with the wrapper (no-op callback since we just want initial states)
        client.subscribe_states(initial_state_helper.on_state_wrapper(lambda _: None))

        # Wait for all initial states to be broadcast
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Get the climate entity and its initial state
        test_climate = climate_infos[0]
        climate_state = initial_state_helper.initial_states.get(test_climate.key)

        assert climate_state is not None, "Climate initial state not found"
        assert isinstance(climate_state, aioesphomeapi.ClimateState)
        assert climate_state.mode == ClimateMode.OFF
        assert climate_state.action == ClimateAction.OFF
        assert climate_state.current_temperature == 22.0
        assert climate_state.target_temperature_low == 18.0
        assert climate_state.target_temperature_high == 24.0
        assert climate_state.preset == ClimatePreset.HOME
        assert climate_state.current_humidity == 42.0
        assert climate_state.target_humidity == 20.0
