"""Integration test for climate custom fan modes and presets."""

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
    """Test that custom fan modes and presets are properly exposed via API."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get entities and services
        entities, services = await client.list_entities_services()
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) == 1, "Expected exactly 1 climate entity"

        test_climate = climate_infos[0]

        # Verify custom fan modes are exposed
        custom_fan_modes = test_climate.supported_custom_fan_modes
        assert len(custom_fan_modes) == 3, (
            f"Expected 3 custom fan modes, got {len(custom_fan_modes)}"
        )
        assert "Turbo" in custom_fan_modes, "Expected 'Turbo' in custom fan modes"
        assert "Silent" in custom_fan_modes, "Expected 'Silent' in custom fan modes"
        assert "Sleep Mode" in custom_fan_modes, (
            "Expected 'Sleep Mode' in custom fan modes"
        )

        # Verify enum presets are exposed (from preset: config map)
        assert ClimatePreset.AWAY in test_climate.supported_presets, (
            "Expected AWAY in enum presets"
        )

        # Verify custom string presets are exposed (from custom_presets: config list)
        custom_presets = test_climate.supported_custom_presets
        assert len(custom_presets) == 3, (
            f"Expected 3 custom presets, got {len(custom_presets)}: {custom_presets}"
        )
        assert "Eco Plus" in custom_presets, "Expected 'Eco Plus' in custom presets"
        assert "Comfort" in custom_presets, "Expected 'Comfort' in custom presets"
        assert "Vacation Mode" in custom_presets, (
            "Expected 'Vacation Mode' in custom presets"
        )
