"""Integration test for many entities to test API batching."""

from __future__ import annotations

import asyncio

from aioesphomeapi import ClimateInfo, EntityState, SensorState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_many_entities(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test API batching with many entities of different types."""
    # Write, compile and run the ESPHome device, then connect to API
    loop = asyncio.get_running_loop()
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Subscribe to state changes
        states: dict[int, EntityState] = {}
        sensor_count_future: asyncio.Future[int] = loop.create_future()

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            # Count sensor states specifically
            sensor_states = [
                s
                for s in states.values()
                if isinstance(s, SensorState) and isinstance(s.state, float)
            ]
            # When we have received states from at least 50 sensors, resolve the future
            if len(sensor_states) >= 50 and not sensor_count_future.done():
                sensor_count_future.set_result(len(sensor_states))

        client.subscribe_states(on_state)

        # Wait for states from at least 50 sensors with timeout
        try:
            sensor_count = await asyncio.wait_for(sensor_count_future, timeout=10.0)
        except TimeoutError:
            sensor_states = [
                s
                for s in states.values()
                if isinstance(s, SensorState) and isinstance(s.state, float)
            ]
            pytest.fail(
                f"Did not receive states from at least 50 sensors within 10 seconds. "
                f"Received {len(sensor_states)} sensor states out of {len(states)} total states"
            )

        # Verify we received a good number of entity states
        assert len(states) >= 50, (
            f"Expected at least 50 total states, got {len(states)}"
        )

        # Verify we have the expected sensor states
        sensor_states = [
            s
            for s in states.values()
            if isinstance(s, SensorState) and isinstance(s.state, float)
        ]

        assert sensor_count >= 50, (
            f"Expected at least 50 sensor states, got {sensor_count}"
        )
        assert len(sensor_states) >= 50, (
            f"Expected at least 50 sensor states, got {len(sensor_states)}"
        )

        # Get entity info to verify climate entity details
        entities = await client.list_entities_services()
        climate_infos = [e for e in entities[0] if isinstance(e, ClimateInfo)]
        assert len(climate_infos) >= 1, "Expected at least 1 climate entity"

        climate_info = climate_infos[0]
        # Verify the thermostat has presets
        assert len(climate_info.supported_presets) > 0, (
            "Expected climate to have presets"
        )
        # The thermostat platform uses standard presets (Home, Away, Sleep)
        # which should be transmitted properly without string copies

        # Verify specific presets exist
        preset_names = [p.name for p in climate_info.supported_presets]
        assert "HOME" in preset_names, f"Expected 'HOME' preset, got {preset_names}"
        assert "AWAY" in preset_names, f"Expected 'AWAY' preset, got {preset_names}"
        assert "SLEEP" in preset_names, f"Expected 'SLEEP' preset, got {preset_names}"
