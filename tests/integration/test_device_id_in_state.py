"""Integration test for device_id in entity state responses."""

from __future__ import annotations

import asyncio

from aioesphomeapi import BinarySensorState, EntityState, SensorState, TextSensorState
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_device_id_in_state(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that device_id is included in entity state responses."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get device info to verify devices are configured
        device_info = await client.device_info()
        assert device_info is not None

        # Verify devices exist
        devices = device_info.devices
        assert len(devices) >= 3, f"Expected at least 3 devices, got {len(devices)}"

        # Get device IDs for verification
        device_ids = {device.name: device.device_id for device in devices}
        assert "Temperature Monitor" in device_ids
        assert "Humidity Monitor" in device_ids
        assert "Motion Sensor" in device_ids

        # Get entity list
        entities = await client.list_entities_services()
        all_entities = entities[0]

        # Create a mapping of entity key to expected device_id
        entity_device_mapping: dict[int, int] = {}

        for entity in all_entities:
            # All entities have name and key attributes
            if entity.name == "Temperature":
                entity_device_mapping[entity.key] = device_ids["Temperature Monitor"]
            elif entity.name == "Humidity":
                entity_device_mapping[entity.key] = device_ids["Humidity Monitor"]
            elif entity.name == "Motion Detected":
                entity_device_mapping[entity.key] = device_ids["Motion Sensor"]
            elif entity.name in {"Temperature Monitor Power", "Temperature Status"}:
                entity_device_mapping[entity.key] = device_ids["Temperature Monitor"]
            elif entity.name == "Motion Light":
                entity_device_mapping[entity.key] = device_ids["Motion Sensor"]
            elif entity.name == "No Device Sensor":
                # Entity without device_id should have device_id 0
                entity_device_mapping[entity.key] = 0

        assert len(entity_device_mapping) >= 6, (
            f"Expected at least 6 mapped entities, got {len(entity_device_mapping)}"
        )

        # Subscribe to states
        loop = asyncio.get_running_loop()
        states: dict[int, EntityState] = {}
        states_future: asyncio.Future[bool] = loop.create_future()

        def on_state(state: EntityState) -> None:
            states[state.key] = state
            # Check if we have states for all mapped entities
            if len(states) >= len(entity_device_mapping) and not states_future.done():
                states_future.set_result(True)

        client.subscribe_states(on_state)

        # Wait for states
        try:
            await asyncio.wait_for(states_future, timeout=10.0)
        except TimeoutError:
            pytest.fail(
                f"Did not receive all entity states within 10 seconds. "
                f"Received {len(states)} states, expected {len(entity_device_mapping)}"
            )

        # Verify each state has the correct device_id
        verified_count = 0
        for key, expected_device_id in entity_device_mapping.items():
            if key in states:
                state = states[key]

                assert state.device_id == expected_device_id, (
                    f"State for key {key} has device_id {state.device_id}, "
                    f"expected {expected_device_id}"
                )
                verified_count += 1

        assert verified_count >= 6, (
            f"Only verified {verified_count} states, expected at least 6"
        )

        # Test specific state types to ensure device_id is present
        # Find a sensor state with device_id
        sensor_state = next(
            (
                s
                for s in states.values()
                if isinstance(s, SensorState)
                and isinstance(s.state, float)
                and s.device_id != 0
            ),
            None,
        )
        assert sensor_state is not None, "No sensor state with device_id found"
        assert sensor_state.device_id > 0, "Sensor state should have non-zero device_id"

        # Find a binary sensor state
        binary_sensor_state = next(
            (s for s in states.values() if isinstance(s, BinarySensorState)),
            None,
        )
        assert binary_sensor_state is not None, "No binary sensor state found"
        assert binary_sensor_state.device_id > 0, (
            "Binary sensor state should have non-zero device_id"
        )

        # Find a text sensor state
        text_sensor_state = next(
            (s for s in states.values() if isinstance(s, TextSensorState)),
            None,
        )
        assert text_sensor_state is not None, "No text sensor state found"
        assert text_sensor_state.device_id > 0, (
            "Text sensor state should have non-zero device_id"
        )

        # Verify the "No Device Sensor" has device_id = 0
        no_device_key = next(
            (key for key, device_id in entity_device_mapping.items() if device_id == 0),
            None,
        )
        assert no_device_key is not None, "No entity mapped to device_id 0"
        assert no_device_key in states, f"State for key {no_device_key} not found"
        no_device_state = states[no_device_key]
        assert no_device_state.device_id == 0, (
            f"Entity without device_id should have device_id=0, got {no_device_state.device_id}"
        )
