"""Integration test for duplicate entity handling with new validation."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityInfo
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_duplicate_entities_on_different_devices(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that duplicate entity names are allowed on different devices."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get device info
        device_info = await client.device_info()
        assert device_info is not None

        # Get devices
        devices = device_info.devices
        assert len(devices) >= 3, f"Expected at least 3 devices, got {len(devices)}"

        # Find our test devices
        controller_1 = next((d for d in devices if d.name == "Controller 1"), None)
        controller_2 = next((d for d in devices if d.name == "Controller 2"), None)
        controller_3 = next((d for d in devices if d.name == "Controller 3"), None)

        assert controller_1 is not None, "Controller 1 device not found"
        assert controller_2 is not None, "Controller 2 device not found"
        assert controller_3 is not None, "Controller 3 device not found"

        # Get entity list
        entities = await client.list_entities_services()
        all_entities: list[EntityInfo] = []
        all_entities.extend(entities[0])

        # Group entities by type for easier testing
        sensors = [e for e in all_entities if e.__class__.__name__ == "SensorInfo"]
        binary_sensors = [
            e for e in all_entities if e.__class__.__name__ == "BinarySensorInfo"
        ]
        text_sensors = [
            e for e in all_entities if e.__class__.__name__ == "TextSensorInfo"
        ]
        switches = [e for e in all_entities if e.__class__.__name__ == "SwitchInfo"]
        buttons = [e for e in all_entities if e.__class__.__name__ == "ButtonInfo"]
        numbers = [e for e in all_entities if e.__class__.__name__ == "NumberInfo"]

        # Scenario 1: Check sensors with same "Temperature" name on different devices
        temp_sensors = [s for s in sensors if s.name == "Temperature"]
        assert len(temp_sensors) == 4, (
            f"Expected exactly 4 temperature sensors, got {len(temp_sensors)}"
        )

        # Verify each sensor is on a different device
        temp_device_ids = set()
        temp_object_ids = set()

        for sensor in temp_sensors:
            temp_device_ids.add(sensor.device_id)
            temp_object_ids.add(sensor.object_id)

            # All should have object_id "temperature" (no suffix)
            assert sensor.object_id == "temperature", (
                f"Expected object_id 'temperature', got '{sensor.object_id}'"
            )

        # Should have 4 different device IDs (including None for main device)
        assert len(temp_device_ids) == 4, (
            f"Temperature sensors should be on different devices, got {temp_device_ids}"
        )

        # Scenario 2: Check binary sensors "Status" on different devices
        status_binary = [b for b in binary_sensors if b.name == "Status"]
        assert len(status_binary) == 3, (
            f"Expected exactly 3 status binary sensors, got {len(status_binary)}"
        )

        # All should have object_id "status"
        for binary in status_binary:
            assert binary.object_id == "status", (
                f"Expected object_id 'status', got '{binary.object_id}'"
            )

        # Scenario 3: Check that sensor and binary_sensor can have same name
        temp_binary = [b for b in binary_sensors if b.name == "Temperature"]
        assert len(temp_binary) == 1, (
            f"Expected exactly 1 temperature binary sensor, got {len(temp_binary)}"
        )
        assert temp_binary[0].object_id == "temperature"

        # Scenario 4: Check text sensors "Device Info" on different devices
        info_text = [t for t in text_sensors if t.name == "Device Info"]
        assert len(info_text) == 3, (
            f"Expected exactly 3 device info text sensors, got {len(info_text)}"
        )

        # All should have object_id "device_info"
        for text in info_text:
            assert text.object_id == "device_info", (
                f"Expected object_id 'device_info', got '{text.object_id}'"
            )

        # Scenario 5: Check switches "Power" on different devices
        power_switches = [s for s in switches if s.name == "Power"]
        assert len(power_switches) == 3, (
            f"Expected exactly 3 power switches, got {len(power_switches)}"
        )

        # All should have object_id "power"
        for switch in power_switches:
            assert switch.object_id == "power", (
                f"Expected object_id 'power', got '{switch.object_id}'"
            )

        # Scenario 6: Check empty name buttons (should use device name)
        empty_buttons = [b for b in buttons if b.name == ""]
        assert len(empty_buttons) == 3, (
            f"Expected exactly 3 empty name buttons, got {len(empty_buttons)}"
        )

        # Group by device
        c1_buttons = [b for b in empty_buttons if b.device_id == controller_1.device_id]
        c2_buttons = [b for b in empty_buttons if b.device_id == controller_2.device_id]

        # For main device, device_id is 0
        main_buttons = [b for b in empty_buttons if b.device_id == 0]

        # Check object IDs for empty name entities
        assert len(c1_buttons) == 1 and c1_buttons[0].object_id == "controller_1"
        assert len(c2_buttons) == 1 and c2_buttons[0].object_id == "controller_2"
        assert (
            len(main_buttons) == 1
            and main_buttons[0].object_id == "duplicate-entities-test"
        )

        # Scenario 7: Check special characters in number names
        temp_numbers = [n for n in numbers if n.name == "Temperature Setpoint!"]
        assert len(temp_numbers) == 2, (
            f"Expected exactly 2 temperature setpoint numbers, got {len(temp_numbers)}"
        )

        # Special characters should be sanitized to _ in object_id
        for number in temp_numbers:
            assert number.object_id == "temperature_setpoint_", (
                f"Expected object_id 'temperature_setpoint_', got '{number.object_id}'"
            )

        # Verify we can get states for all entities (ensures they're functional)
        loop = asyncio.get_running_loop()
        states_future: asyncio.Future[None] = loop.create_future()
        state_count = 0
        expected_count = (
            len(sensors)
            + len(binary_sensors)
            + len(text_sensors)
            + len(switches)
            + len(buttons)
            + len(numbers)
        )

        def on_state(state) -> None:
            nonlocal state_count
            state_count += 1
            if state_count >= expected_count and not states_future.done():
                states_future.set_result(None)

        client.subscribe_states(on_state)

        # Wait for all entity states
        try:
            await asyncio.wait_for(states_future, timeout=10.0)
        except TimeoutError:
            pytest.fail(
                f"Did not receive all entity states within 10 seconds. "
                f"Expected {expected_count}, received {state_count}"
            )
