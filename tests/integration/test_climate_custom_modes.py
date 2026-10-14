"""Integration test for climate custom presets."""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import ClimateInfo, ClimatePreset, EntityState
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_climate_custom_fan_modes_and_presets(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that custom presets are properly exposed and can be changed."""
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        states: dict[int, EntityState] = {}
        super_saver_future: asyncio.Future[EntityState] = loop.create_future()
        vacation_future: asyncio.Future[EntityState] = loop.create_future()

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            if isinstance(state, aioesphomeapi.ClimateState):
                # Wait for Super Saver preset
                if (
                    state.custom_preset == "Super Saver"
                    and state.target_temperature_low == 20.0
                    and state.target_temperature_high == 24.0
                    and not super_saver_future.done()
                ):
                    super_saver_future.set_result(state)
                # Wait for Vacation Mode preset
                elif (
                    state.custom_preset == "Vacation Mode"
                    and state.target_temperature_low == 15.0
                    and state.target_temperature_high == 18.0
                    and not vacation_future.done()
                ):
                    vacation_future.set_result(state)

        # Get entities and set up state synchronization
        entities, services = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) == 1, "Expected exactly 1 climate entity"

        test_climate = climate_infos[0]

        # Subscribe with the wrapper that filters initial states
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        # Wait for all initial states to be broadcast
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Verify enum presets are exposed (from preset: config map)
        assert ClimatePreset.AWAY in test_climate.supported_presets, (
            "Expected AWAY in enum presets"
        )

        # Verify custom string presets are exposed (non-standard preset names from preset map)
        custom_presets = test_climate.supported_custom_presets
        assert len(custom_presets) == 3, (
            f"Expected 3 custom presets, got {len(custom_presets)}: {custom_presets}"
        )
        assert "Eco Plus" in custom_presets, "Expected 'Eco Plus' in custom presets"
        assert "Super Saver" in custom_presets, (
            "Expected 'Super Saver' in custom presets"
        )
        assert "Vacation Mode" in custom_presets, (
            "Expected 'Vacation Mode' in custom presets"
        )

        # Get initial state and verify default preset
        initial_state = initial_state_helper.initial_states.get(test_climate.key)
        assert initial_state is not None, "Climate initial state not found"
        assert isinstance(initial_state, aioesphomeapi.ClimateState)
        assert initial_state.custom_preset == "Eco Plus", (
            f"Expected default preset 'Eco Plus', got '{initial_state.custom_preset}'"
        )
        assert initial_state.target_temperature_low == 18.0, (
            f"Expected low temp 18.0, got {initial_state.target_temperature_low}"
        )
        assert initial_state.target_temperature_high == 22.0, (
            f"Expected high temp 22.0, got {initial_state.target_temperature_high}"
        )

        # Test changing to "Super Saver" custom preset
        client.climate_command(test_climate.key, custom_preset="Super Saver")

        try:
            super_saver_state = await asyncio.wait_for(super_saver_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Super Saver preset change not received within 5 seconds")

        assert isinstance(super_saver_state, aioesphomeapi.ClimateState)
        assert super_saver_state.custom_preset == "Super Saver"
        assert super_saver_state.target_temperature_low == 20.0
        assert super_saver_state.target_temperature_high == 24.0

        # Test changing to "Vacation Mode" custom preset
        client.climate_command(test_climate.key, custom_preset="Vacation Mode")

        try:
            vacation_state = await asyncio.wait_for(vacation_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Vacation Mode preset change not received within 5 seconds")

        assert isinstance(vacation_state, aioesphomeapi.ClimateState)
        assert vacation_state.custom_preset == "Vacation Mode"
        assert vacation_state.target_temperature_low == 15.0
        assert vacation_state.target_temperature_high == 18.0
