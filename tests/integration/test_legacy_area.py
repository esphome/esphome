"""Integration test for legacy string-based area configuration."""

from __future__ import annotations

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_legacy_area(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test legacy string-based area configuration."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get device info which includes areas
        device_info = await client.device_info()
        assert device_info is not None

        # Verify the area is reported (should be converted to structured format)
        areas = device_info.areas
        assert len(areas) == 1, f"Expected exactly 1 area, got {len(areas)}"

        # Find the area - should be slugified from "Master Bedroom"
        area = areas[0]
        assert area.name == "Master Bedroom", (
            f"Expected area name 'Master Bedroom', got '{area.name}'"
        )

        # Verify area.id is set (it should be a hash)
        assert area.area_id > 0, "Area ID should be a positive hash value"

        # The suggested_area field should be set for backward compatibility
        assert device_info.suggested_area == "Master Bedroom", (
            f"Expected suggested_area to be 'Master Bedroom', got '{device_info.suggested_area}'"
        )

        # Verify deprecated warning would have been logged during compilation
        # (We can't check logs directly in integration tests, but the code should work)
