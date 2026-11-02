"""Integration test for many entities to test API batching."""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    ClimateFanMode,
    ClimateFeature,
    ClimateInfo,
    ClimateMode,
    DateInfo,
    DateState,
    DateTimeInfo,
    DateTimeState,
    EntityState,
    SensorState,
    TimeInfo,
    TimeState,
)
import pytest

from .state_utils import require_only_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail


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
        minimum_states_future: asyncio.Future[None] = loop.create_future()

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            # Check if we have received minimum expected states
            sensor_states = [
                s
                for s in states.values()
                if isinstance(s, SensorState) and isinstance(s.state, float)
            ]
            date_states = [s for s in states.values() if isinstance(s, DateState)]
            time_states = [s for s in states.values() if isinstance(s, TimeState)]
            datetime_states = [
                s for s in states.values() if isinstance(s, DateTimeState)
            ]

            # We expect at least 50 sensors and 1 of each datetime entity type
            if (
                len(sensor_states) >= 50
                and len(date_states) >= 1
                and len(time_states) >= 1
                and len(datetime_states) >= 1
                and not minimum_states_future.done()
            ):
                minimum_states_future.set_result(None)

        client.subscribe_states(on_state)

        # Wait for minimum states with timeout
        def count(kind: type) -> int:
            return sum(isinstance(s, kind) for s in states.values())

        await wait_or_fail(
            minimum_states_future,
            10.0,
            "Did not receive the expected states (>=50 sensor, >=1 date, time and datetime)",
            sensor=lambda: count(SensorState),
            date=lambda: count(DateState),
            time=lambda: count(TimeState),
            datetime=lambda: count(DateTimeState),
            total=lambda: len(states),
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

        assert len(sensor_states) >= 50, (
            f"Expected at least 50 sensor states, got {len(sensor_states)}"
        )

        # Verify we received datetime entity states
        date_states = [s for s in states.values() if isinstance(s, DateState)]
        time_states = [s for s in states.values() if isinstance(s, TimeState)]
        datetime_states = [s for s in states.values() if isinstance(s, DateTimeState)]

        assert len(date_states) >= 1, (
            f"Expected at least 1 date state, got {len(date_states)}"
        )
        assert len(time_states) >= 1, (
            f"Expected at least 1 time state, got {len(time_states)}"
        )
        assert len(datetime_states) >= 1, (
            f"Expected at least 1 datetime state, got {len(datetime_states)}"
        )

        # Get entity info to verify climate entity details
        entities = await client.list_entities_services()
        climate_info = require_only_entity(entities[0], ClimateInfo)

        # Verify feature flags set as expected
        assert climate_info.feature_flags == (
            ClimateFeature.SUPPORTS_ACTION
            | ClimateFeature.SUPPORTS_CURRENT_HUMIDITY
            | ClimateFeature.SUPPORTS_CURRENT_TEMPERATURE
            | ClimateFeature.SUPPORTS_TWO_POINT_TARGET_TEMPERATURE
            | ClimateFeature.SUPPORTS_TARGET_HUMIDITY
        )

        # Verify modes
        assert climate_info.supported_modes == [
            ClimateMode.OFF,
            ClimateMode.COOL,
            ClimateMode.HEAT,
        ], f"Expected modes [OFF, COOL, HEAT], got {climate_info.supported_modes}"

        # Verify visual parameters
        assert climate_info.visual_min_temperature == 15.0, (
            f"Expected min_temperature=15.0, got {climate_info.visual_min_temperature}"
        )
        assert climate_info.visual_max_temperature == 32.0, (
            f"Expected max_temperature=32.0, got {climate_info.visual_max_temperature}"
        )
        assert climate_info.visual_target_temperature_step == 0.1, (
            f"Expected temperature_step=0.1, got {climate_info.visual_target_temperature_step}"
        )
        assert climate_info.visual_min_humidity == 20.0, (
            f"Expected min_humidity=20.0, got {climate_info.visual_min_humidity}"
        )
        assert climate_info.visual_max_humidity == 70.0, (
            f"Expected max_humidity=70.0, got {climate_info.visual_max_humidity}"
        )

        # Verify fan modes
        assert climate_info.supported_fan_modes == [
            ClimateFanMode.ON,
            ClimateFanMode.AUTO,
        ], f"Expected fan modes [ON, AUTO], got {climate_info.supported_fan_modes}"

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

        # Verify the datetime entities and their names
        date_info = require_only_entity(entities[0], DateInfo)
        assert date_info.name == "Test Date", (
            f"Expected date entity name 'Test Date', got {date_info.name}"
        )

        time_info = require_only_entity(entities[0], TimeInfo)
        assert time_info.name == "Test Time", (
            f"Expected time entity name 'Test Time', got {time_info.name}"
        )

        datetime_info = require_only_entity(entities[0], DateTimeInfo)
        assert datetime_info.name == "Test DateTime", (
            f"Expected datetime entity name 'Test DateTime', got {datetime_info.name}"
        )
