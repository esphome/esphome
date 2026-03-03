"""Integration test for areas and devices feature."""

from __future__ import annotations

import asyncio

from aioesphomeapi import EntityState, SwitchInfo, SwitchState
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

        # Collect sensor entities (all entities have device_id)
        sensor_entities = entities[0]
        assert len(sensor_entities) >= 4, (
            f"Expected at least 4 sensor entities, got {len(sensor_entities)}"
        )

        # Subscribe to states to get sensor values
        loop = asyncio.get_running_loop()
        states: dict[tuple[int, int], EntityState] = {}
        # Subscribe to all switch states
        switch_state_futures: dict[
            tuple[int, int], asyncio.Future[EntityState]
        ] = {}  # (device_id, key) -> future
        initial_states_received: set[tuple[int, int]] = set()
        initial_states_future: asyncio.Future[bool] = loop.create_future()

        def on_state(state: EntityState) -> None:
            state_key = (state.device_id, state.key)
            states[state_key] = state

            initial_states_received.add(state_key)
            # Check if we have all initial states
            if (
                len(initial_states_received) >= 8  # 8 entities expected
                and not initial_states_future.done()
            ):
                initial_states_future.set_result(True)

            if not initial_states_future.done():
                return

            # Resolve the future for this switch if it exists
            if (
                state_key in switch_state_futures
                and not switch_state_futures[state_key].done()
                and isinstance(state, SwitchState)
            ):
                switch_state_futures[state_key].set_result(state)

        client.subscribe_states(on_state)

        # Wait for sensor states
        try:
            await asyncio.wait_for(initial_states_future, timeout=10.0)
        except TimeoutError:
            pytest.fail(
                f"Did not receive all states within 10 seconds. "
                f"Received {len(states)} states"
            )

        # Verify we have sensor entities with proper device_id assignments
        device_id_mapping = {
            "Light Controller Sensor": light_controller.device_id,
            "Temperature Sensor Reading": temp_sensor.device_id,
            "Motion Detector Status": motion_detector.device_id,
            "Smart Switch Power": smart_switch.device_id,
            "Living Room Sensor": 0,  # Main device
        }

        for entity in sensor_entities:
            if entity.name in device_id_mapping:
                expected_device_id = device_id_mapping[entity.name]
                assert entity.device_id == expected_device_id, (
                    f"{entity.name} has device_id {entity.device_id}, "
                    f"expected {expected_device_id}"
                )

        all_entities, _ = entities  # Unpack the tuple
        switch_entities = [e for e in all_entities if isinstance(e, SwitchInfo)]

        # Find all switches named "Test Switch"
        test_switches = [e for e in switch_entities if e.name == "Test Switch"]
        assert len(test_switches) == 4, (
            f"Expected 4 'Test Switch' entities, got {len(test_switches)}"
        )

        # Verify we have switches with different device_ids including one with 0 (main)
        switch_device_ids = {s.device_id for s in test_switches}
        assert len(switch_device_ids) == 4, (
            "All Test Switch entities should have different device_ids"
        )
        assert 0 in switch_device_ids, (
            "Should have a switch with device_id 0 (main device)"
        )

        # Verify extra switches with blank and none device_id are correctly available
        extra_switches = [
            e for e in switch_entities if e.name.startswith("Living Room")
        ]
        assert len(extra_switches) == 2, (
            f"Expected 2 extra switches for Living Room, got {len(extra_switches)}"
        )
        extra_switch_device_ids = [e.device_id for e in extra_switches]
        assert all(d == 0 for d in extra_switch_device_ids), (
            "All extra switches should have device_id 0 (main device)"
        )

        # Wait for initial states to be received for all switches
        await asyncio.wait_for(initial_states_future, timeout=2.0)

        # Test controlling each switch specifically by device_id
        for device_name, device in [
            ("Light Controller", light_controller),
            ("Temperature Sensor", temp_sensor),
            ("Motion Detector", motion_detector),
        ]:
            # Find the switch for this specific device
            device_switch = next(
                (s for s in test_switches if s.device_id == device.device_id), None
            )
            assert device_switch is not None, f"No Test Switch found for {device_name}"

            # Create future for this switch's state change
            state_key = (device_switch.device_id, device_switch.key)
            switch_state_futures[state_key] = loop.create_future()

            # Turn on the switch with device_id
            client.switch_command(
                device_switch.key, True, device_id=device_switch.device_id
            )

            # Wait for state to change
            await asyncio.wait_for(switch_state_futures[state_key], timeout=2.0)

            # Verify the correct switch was turned on
            assert states[state_key].state is True, f"{device_name} switch should be on"

            # Create new future for turning off
            switch_state_futures[state_key] = loop.create_future()

            # Turn off the switch with device_id
            client.switch_command(
                device_switch.key, False, device_id=device_switch.device_id
            )

            # Wait for state to change
            await asyncio.wait_for(switch_state_futures[state_key], timeout=2.0)

            # Verify the correct switch was turned off
            assert states[state_key].state is False, (
                f"{device_name} switch should be off"
            )

        # Test that controlling a switch with device_id doesn't affect main switch
        # Find the main switch (device_id = 0)
        main_switch = next((s for s in test_switches if s.device_id == 0), None)
        assert main_switch is not None, "No main switch (device_id=0) found"

        # Find a switch with a device_id
        device_switch = next(
            (s for s in test_switches if s.device_id == light_controller.device_id),
            None,
        )
        assert device_switch is not None, "No device switch found"

        # Create futures for both switches
        main_key = (main_switch.device_id, main_switch.key)
        device_key = (device_switch.device_id, device_switch.key)

        # Turn on the main switch first
        switch_state_futures[main_key] = loop.create_future()
        client.switch_command(main_switch.key, True, device_id=main_switch.device_id)
        await asyncio.wait_for(switch_state_futures[main_key], timeout=2.0)
        assert states[main_key].state is True, "Main switch should be on"

        # Now turn on the device switch
        switch_state_futures[device_key] = loop.create_future()
        client.switch_command(
            device_switch.key, True, device_id=device_switch.device_id
        )
        await asyncio.wait_for(switch_state_futures[device_key], timeout=2.0)

        # Verify device switch is on and main switch is still on
        assert states[device_key].state is True, "Device switch should be on"
        assert states[main_key].state is True, (
            "Main switch should still be on after turning on device switch"
        )

        # Turn off the device switch
        switch_state_futures[device_key] = loop.create_future()
        client.switch_command(
            device_switch.key, False, device_id=device_switch.device_id
        )
        await asyncio.wait_for(switch_state_futures[device_key], timeout=2.0)

        # Verify device switch is off and main switch is still on
        assert states[device_key].state is False, "Device switch should be off"
        assert states[main_key].state is True, (
            "Main switch should still be on after turning off device switch"
        )

        # Clean up - turn off main switch
        switch_state_futures[main_key] = loop.create_future()
        client.switch_command(main_switch.key, False, device_id=main_switch.device_id)
        await asyncio.wait_for(switch_state_futures[main_key], timeout=2.0)
        assert states[main_key].state is False, "Main switch should be off"
