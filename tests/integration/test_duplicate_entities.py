"""Integration test for duplicate entity handling with new validation."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityInfo
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_duplicate_entities_not_allowed_on_different_devices(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that duplicate entity names are NOT allowed on different devices."""
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
        for entity_list in entities[0]:
            all_entities.append(entity_list)

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
        selects = [e for e in all_entities if e.__class__.__name__ == "SelectInfo"]

        # Scenario 1: Check that temperature sensors have unique names per device
        temp_sensors = [s for s in sensors if "Temperature" in s.name]
        assert len(temp_sensors) == 4, (
            f"Expected exactly 4 temperature sensors, got {len(temp_sensors)}"
        )

        # Verify each sensor has a unique name
        temp_names = set()
        temp_object_ids = set()

        for sensor in temp_sensors:
            temp_names.add(sensor.name)
            temp_object_ids.add(sensor.object_id)

        # Should have 4 unique names
        assert len(temp_names) == 4, (
            f"Temperature sensors should have unique names, got {temp_names}"
        )

        # Object IDs should also be unique
        assert len(temp_object_ids) == 4, (
            f"Temperature sensors should have unique object_ids, got {temp_object_ids}"
        )

        # Scenario 2: Check binary sensors have unique names
        status_binary = [b for b in binary_sensors if "Status" in b.name]
        assert len(status_binary) == 3, (
            f"Expected exactly 3 status binary sensors, got {len(status_binary)}"
        )

        # All should have unique object_ids
        status_names = set()
        for binary in status_binary:
            status_names.add(binary.name)

        assert len(status_names) == 3, (
            f"Status binary sensors should have unique names, got {status_names}"
        )

        # Scenario 3: Check that sensor and binary_sensor can have same name
        temp_binary = [b for b in binary_sensors if b.name == "Temperature"]
        assert len(temp_binary) == 1, (
            f"Expected exactly 1 temperature binary sensor, got {len(temp_binary)}"
        )
        assert temp_binary[0].object_id == "temperature"

        # Scenario 4: Check text sensors have unique names
        info_text = [t for t in text_sensors if "Device Info" in t.name]
        assert len(info_text) == 3, (
            f"Expected exactly 3 device info text sensors, got {len(info_text)}"
        )

        # All should have unique names and object_ids
        info_names = set()
        for text in info_text:
            info_names.add(text.name)

        assert len(info_names) == 3, (
            f"Device info text sensors should have unique names, got {info_names}"
        )

        # Scenario 5: Check switches have unique names
        power_switches = [s for s in switches if "Power" in s.name]
        assert len(power_switches) == 4, (
            f"Expected exactly 4 power switches, got {len(power_switches)}"
        )

        # All should have unique names
        power_names = set()
        for switch in power_switches:
            power_names.add(switch.name)

        assert len(power_names) == 4, (
            f"Power switches should have unique names, got {power_names}"
        )

        # Scenario 6: Check reset buttons have unique names
        reset_buttons = [b for b in buttons if "Reset" in b.name]
        assert len(reset_buttons) == 3, (
            f"Expected exactly 3 reset buttons, got {len(reset_buttons)}"
        )

        # All should have unique names
        reset_names = set()
        for button in reset_buttons:
            reset_names.add(button.name)

        assert len(reset_names) == 3, (
            f"Reset buttons should have unique names, got {reset_names}"
        )

        # Scenario 7: Check empty name selects (should use device names)
        empty_selects = [s for s in selects if s.name == ""]
        assert len(empty_selects) == 3, (
            f"Expected exactly 3 empty name selects, got {len(empty_selects)}"
        )

        # Group by device
        c1_selects = [s for s in empty_selects if s.device_id == controller_1.device_id]
        c2_selects = [s for s in empty_selects if s.device_id == controller_2.device_id]

        # For main device, device_id is 0
        main_selects = [s for s in empty_selects if s.device_id == 0]

        # Check object IDs for empty name entities - they should use device names
        assert len(c1_selects) == 1 and c1_selects[0].object_id == "controller_1"
        assert len(c2_selects) == 1 and c2_selects[0].object_id == "controller_2"
        assert (
            len(main_selects) == 1
            and main_selects[0].object_id == "duplicate-entities-test"
        )

        # Scenario 8: Check special characters in number names - now unique
        temp_numbers = [n for n in numbers if "Temperature Setpoint!" in n.name]
        assert len(temp_numbers) == 2, (
            f"Expected exactly 2 temperature setpoint numbers, got {len(temp_numbers)}"
        )

        # Should have unique names
        setpoint_names = set()
        for number in temp_numbers:
            setpoint_names.add(number.name)

        assert len(setpoint_names) == 2, (
            f"Temperature setpoint numbers should have unique names, got {setpoint_names}"
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
            + len(selects)
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
