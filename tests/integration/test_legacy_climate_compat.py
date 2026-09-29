"""Integration test for backward compatibility of deprecated ClimateTraits setters.

Verifies that external components using the old traits.set_supported_custom_fan_modes()
and traits.set_supported_custom_presets() API still work correctly during the
deprecation period.

Remove this entire test file and the legacy_climate_component external component
in 2026.11.0 when the deprecated ClimateTraits setters are removed.
"""

from __future__ import annotations

import asyncio
from pathlib import Path

import aioesphomeapi
from aioesphomeapi import ClimateInfo
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_legacy_climate_compat(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that deprecated ClimateTraits custom mode setters still work end-to-end."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()

    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)

        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) == 1, (
            f"Expected 1 climate entity, got {len(climate_infos)}"
        )

        test_climate = climate_infos[0]

        # Verify custom fan modes set via deprecated ClimateTraits setter are exposed
        assert set(test_climate.supported_custom_fan_modes) == {
            "Turbo",
            "Silent",
            "Auto",
        }, (
            f"Expected custom fan modes {{Turbo, Silent, Auto}}, "
            f"got {test_climate.supported_custom_fan_modes}"
        )

        # Verify custom presets set via deprecated ClimateTraits setter are exposed
        assert set(test_climate.supported_custom_presets) == {
            "Eco Mode",
            "Night Mode",
        }, (
            f"Expected custom presets {{Eco Mode, Night Mode}}, "
            f"got {test_climate.supported_custom_presets}"
        )

        # Set up state tracking with InitialStateHelper
        turbo_future: asyncio.Future[aioesphomeapi.ClimateState] = loop.create_future()

        def on_state(state: aioesphomeapi.EntityState) -> None:
            if (
                isinstance(state, aioesphomeapi.ClimateState)
                and state.custom_fan_mode == "Turbo"
                and not turbo_future.done()
            ):
                turbo_future.set_result(state)

        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Verify we can set a custom fan mode via API (tests find_custom_fan_mode_ compat path)
        client.climate_command(test_climate.key, custom_fan_mode="Turbo")

        try:
            turbo_state = await asyncio.wait_for(turbo_future, timeout=5.0)
        except TimeoutError:
            pytest.fail("Custom fan mode 'Turbo' not received within 5 seconds")

        assert turbo_state.custom_fan_mode == "Turbo"
