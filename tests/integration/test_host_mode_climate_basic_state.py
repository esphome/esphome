"""Integration test for Host mode with climate."""

from __future__ import annotations

from aioesphomeapi import (
    ClimateAction,
    ClimateInfo,
    ClimateMode,
    ClimatePreset,
    ClimateState,
    EntityState,
)
import pytest

from .state_utils import wait_for_state
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_climate_basic_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test basic climate state reporting."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) >= 1, "Expected at least 1 climate entity"
        test_climate = climate_infos[0]

        # The thermostat publishes multiple states during setup as the
        # temperature/humidity sensors come online. Wait for the state to
        # converge to the expected default values rather than relying on
        # whichever state happens to arrive first.
        def is_default_state(state: EntityState) -> bool:
            return (
                isinstance(state, ClimateState)
                and state.key == test_climate.key
                and state.mode == ClimateMode.OFF
                and state.action == ClimateAction.OFF
                and state.current_temperature == 22.0
                and state.target_temperature_low == 18.0
                and state.target_temperature_high == 24.0
                and state.preset == ClimatePreset.HOME
                and state.current_humidity == 42.0
                and state.target_humidity == 20.0
            )

        try:
            await wait_for_state(client, is_default_state)
        except TimeoutError:
            pytest.fail("Climate did not converge to expected default state")
