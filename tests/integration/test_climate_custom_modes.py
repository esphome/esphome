"""Integration test for climate custom presets."""

from __future__ import annotations

import aioesphomeapi
from aioesphomeapi import ClimateInfo, ClimatePreset
import pytest

from .state_utils import require_only_entity, subscribe_after_initial_states
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_climate_custom_fan_modes_and_presets(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that custom presets are properly exposed and can be changed."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get entities and set up state synchronization
        entities, services = await client.list_entities_services()
        test_climate = require_only_entity(entities, ClimateInfo)

        # Wait for all initial states to be broadcast
        sub = await subscribe_after_initial_states(client, entities)

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
        initial_state = sub.initial_states.get(test_climate.key)
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
        super_saver_seen = sub.waiter.expect(
            lambda s: (
                isinstance(s, aioesphomeapi.ClimateState)
                and s.custom_preset == "Super Saver"
                and s.target_temperature_low == 20.0
                and s.target_temperature_high == 24.0
            ),
            label="Super Saver preset change",
        )
        client.climate_command(test_climate.key, custom_preset="Super Saver")
        super_saver_state = await super_saver_seen

        assert isinstance(super_saver_state, aioesphomeapi.ClimateState)
        assert super_saver_state.custom_preset == "Super Saver"
        assert super_saver_state.target_temperature_low == 20.0
        assert super_saver_state.target_temperature_high == 24.0

        # Test changing to "Vacation Mode" custom preset
        vacation_seen = sub.waiter.expect(
            lambda s: (
                isinstance(s, aioesphomeapi.ClimateState)
                and s.custom_preset == "Vacation Mode"
                and s.target_temperature_low == 15.0
                and s.target_temperature_high == 18.0
            ),
            label="Vacation Mode preset change",
        )
        client.climate_command(test_climate.key, custom_preset="Vacation Mode")
        vacation_state = await vacation_seen

        assert isinstance(vacation_state, aioesphomeapi.ClimateState)
        assert vacation_state.custom_preset == "Vacation Mode"
        assert vacation_state.target_temperature_low == 15.0
        assert vacation_state.target_temperature_high == 18.0
