"""Integration test for ordered climate fan-mode serialization."""

from __future__ import annotations

from pathlib import Path

from aioesphomeapi import ClimateFanMode, ClimateInfo
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_climate_fan_mode_order(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that supported fan modes preserve the order defined by the component."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) == 1, (
            f"Expected 1 climate entity, got {len(climate_infos)}"
        )

        ordered_climate = climate_infos[0]
        assert ordered_climate.supported_fan_modes == [
            ClimateFanMode.HIGH,
            ClimateFanMode.AUTO,
            ClimateFanMode.LOW,
        ], (
            "Expected supported fan modes to preserve component-defined order, "
            f"got {ordered_climate.supported_fan_modes}"
        )
