"""Integration test for climate custom presets."""

from __future__ import annotations

from aioesphomeapi import ClimateInfo, ClimatePreset
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_climate_custom_fan_modes_and_presets(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that custom presets are properly exposed via API."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get entities and services
        entities, services = await client.list_entities_services()
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) == 1, "Expected exactly 1 climate entity"

        test_climate = climate_infos[0]

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
