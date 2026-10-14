"""Integration test for backward compatibility of deprecated FanTraits setters.

Verifies that external components using the old traits.set_supported_preset_modes()
API still work correctly during the deprecation period.

Remove this entire test file and the legacy_fan_component external component
in 2026.11.0 when the deprecated FanTraits setters are removed.
"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import FanInfo, FanState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_legacy_fan_compat(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that deprecated FanTraits preset mode setters still work end-to-end."""
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    async with run_compiled(yaml_config), api_client_connected() as client:
        entities, _ = await client.list_entities_services()

        fan_infos = [e for e in entities if isinstance(e, FanInfo)]
        assert len(fan_infos) == 1, f"Expected 1 fan entity, got {len(fan_infos)}"

        test_fan = fan_infos[0]

        # Verify preset modes set via deprecated FanTraits setter are exposed
        assert set(test_fan.supported_preset_modes) == {
            "Turbo",
            "Silent",
            "Eco",
        }, (
            f"Expected preset modes {{Turbo, Silent, Eco}}, "
            f"got {test_fan.supported_preset_modes}"
        )

        # Verify speed support
        assert test_fan.supports_speed is True
        assert test_fan.supported_speed_count == 3

        # Subscribe and wait for initial states
        states: dict[int, FanState] = {}
        state_event = asyncio.Event()

        def on_state(state: FanState) -> None:
            if isinstance(state, FanState):
                states[state.key] = state
                state_event.set()

        client.subscribe_states(on_state)

        # Wait for initial state
        await asyncio.wait_for(state_event.wait(), timeout=5.0)

        # Turn on fan with preset mode (tests find_preset_mode_ compat path)
        state_event.clear()
        client.fan_command(
            key=test_fan.key,
            state=True,
            preset_mode="Turbo",
        )
        await asyncio.wait_for(state_event.wait(), timeout=5.0)

        fan_state = states[test_fan.key]
        assert fan_state.state is True
        assert fan_state.preset_mode == "Turbo"
