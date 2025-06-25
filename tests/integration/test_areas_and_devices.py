"""Integration test for areas and devices feature."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_areas_and_devices(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test areas and devices configuration with entity mapping."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get device info which includes areas and devices
        device_info = await client.device_info()
        assert device_info is not None

        # Verify areas are reported
        areas = device_info.areas
        assert len(areas) >= 2, f"Expected at least 2 areas, got {len(areas)}"

        # Find our specific areas
        main_area = next((a for a in areas if a.name == "Living Room"), None)
        bedroom_area = next((a for a in areas if a.name == "Bedroom"), None)
        kitchen_area = next((a for a in areas if a.name == "Kitchen"), None)

        assert main_area is not None, "Living Room area not found"
        assert bedroom_area is not None, "Bedroom area not found"
        assert kitchen_area is not None, "Kitchen area not found"

        # Verify devices are reported
        devices = device_info.devices
        assert len(devices) >= 4, f"Expected at least 4 devices, got {len(devices)}"

        # Find our specific devices
        light_controller = next(
            (d for d in devices if d.name == "Light Controller"), None
        )
        temp_sensor = next((d for d in devices if d.name == "Temperature Sensor"), None)
        motion_detector = next(
            (d for d in devices if d.name == "Motion Detector"), None
        )
        smart_switch = next((d for d in devices if d.name == "Smart Switch"), None)

        assert light_controller is not None, "Light Controller device not found"
        assert temp_sensor is not None, "Temperature Sensor device not found"
        assert motion_detector is not None, "Motion Detector device not found"
        assert smart_switch is not None, "Smart Switch device not found"

        # Verify device area assignments
        assert light_controller.area_id == main_area.area_id, (
            "Light Controller should be in Living Room"
        )
        assert temp_sensor.area_id == bedroom_area.area_id, (
            "Temperature Sensor should be in Bedroom"
        )
        assert motion_detector.area_id == main_area.area_id, (
            "Motion Detector should be in Living Room"
        )
        assert smart_switch.area_id == kitchen_area.area_id, (
            "Smart Switch should be in Kitchen"
        )

        # Verify suggested_area is set to the top-level area name
        assert device_info.suggested_area == "Living Room", (
            f"Expected suggested_area to be 'Living Room', got '{device_info.suggested_area}'"
        )

        # Get entity list to verify device_id mapping
        entities = await client.list_entities_services()

        # Collect sensor entities
        sensor_entities = [e for e in entities[0] if hasattr(e, "device_id")]
        assert len(sensor_entities) >= 4, (
            f"Expected at least 4 sensor entities, got {len(sensor_entities)}"
        )

        # Subscribe to states to get sensor values
        loop = asyncio.get_running_loop()
        states: dict[int, EntityState] = {}
        states_future: asyncio.Future[bool] = loop.create_future()

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            # Check if we have all expected sensor states
            if len(states) >= 4 and not states_future.done():
                states_future.set_result(True)

        client.subscribe_states(on_state)

        # Wait for sensor states
        try:
            await asyncio.wait_for(states_future, timeout=10.0)
        except asyncio.TimeoutError:
            pytest.fail(
                f"Did not receive all sensor states within 10 seconds. "
                f"Received {len(states)} states"
            )

        # Verify we have sensor entities with proper device_id assignments
        device_id_mapping = {
            "Light Controller Sensor": light_controller.device_id,
            "Temperature Sensor Reading": temp_sensor.device_id,
            "Motion Detector Status": motion_detector.device_id,
            "Smart Switch Power": smart_switch.device_id,
        }

        for entity in sensor_entities:
            if entity.name in device_id_mapping:
                expected_device_id = device_id_mapping[entity.name]
                assert entity.device_id == expected_device_id, (
                    f"{entity.name} has device_id {entity.device_id}, "
                    f"expected {expected_device_id}"
                )
