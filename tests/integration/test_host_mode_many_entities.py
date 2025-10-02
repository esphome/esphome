"""Integration test for many entities to test API batching."""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    ClimateInfo,
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
        try:
            await asyncio.wait_for(minimum_states_future, timeout=10.0)
        except TimeoutError:
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

            pytest.fail(
                f"Did not receive expected states within 10 seconds. "
                f"Received: {len(sensor_states)} sensor states (expected >=50), "
                f"{len(date_states)} date states (expected >=1), "
                f"{len(time_states)} time states (expected >=1), "
                f"{len(datetime_states)} datetime states (expected >=1). "
                f"Total states: {len(states)}"
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

        # Verify datetime entities exist
        date_infos = [e for e in entities[0] if isinstance(e, DateInfo)]
        time_infos = [e for e in entities[0] if isinstance(e, TimeInfo)]
        datetime_infos = [e for e in entities[0] if isinstance(e, DateTimeInfo)]

        assert len(date_infos) >= 1, "Expected at least 1 date entity"
        assert len(time_infos) >= 1, "Expected at least 1 time entity"
        assert len(datetime_infos) >= 1, "Expected at least 1 datetime entity"

        # Verify the entity names
        date_info = date_infos[0]
        assert date_info.name == "Test Date", (
            f"Expected date entity name 'Test Date', got {date_info.name}"
        )

        time_info = time_infos[0]
        assert time_info.name == "Test Time", (
            f"Expected time entity name 'Test Time', got {time_info.name}"
        )

        datetime_info = datetime_infos[0]
        assert datetime_info.name == "Test DateTime", (
            f"Expected datetime entity name 'Test DateTime', got {datetime_info.name}"
        )
