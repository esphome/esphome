"""Integration test for device_id in entity state responses."""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    AlarmControlPanelEntityState,
    BinarySensorState,
    CoverState,
    DateState,
    DateTimeState,
    EntityState,
    FanState,
    LightState,
    LockEntityState,
    NumberState,
    SelectState,
    SensorState,
    SwitchState,
    TextSensorState,
    TextState,
    TimeState,
    ValveState,
    WaterHeaterState,
)
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

# Mapping of entity name to device name for all entities with device_id
ENTITY_TO_DEVICE = {
    # Original entities
    "Temperature": "Temperature Monitor",
    "Humidity": "Humidity Monitor",
    "Motion Detected": "Motion Sensor",
    "Temperature Monitor Power": "Temperature Monitor",
    "Temperature Status": "Temperature Monitor",
    "Motion Light": "Motion Sensor",
    # New entity types
    "Garage Door": "Motion Sensor",
    "Ceiling Fan": "Humidity Monitor",
    "Front Door Lock": "Motion Sensor",
    "Target Temperature": "Temperature Monitor",
    "Mode Select": "Humidity Monitor",
    "Device Label": "Temperature Monitor",
    "Water Valve": "Humidity Monitor",
    "Test Boiler": "Temperature Monitor",
    "House Alarm": "Motion Sensor",
    "Schedule Date": "Temperature Monitor",
    "Schedule Time": "Humidity Monitor",
    "Schedule DateTime": "Motion Sensor",
    "Doorbell": "Motion Sensor",
}

# Entities without device_id (should have device_id 0)
NO_DEVICE_ENTITIES = {"No Device Sensor"}

# State types that should have non-zero device_id, mapped by their aioesphomeapi class
EXPECTED_STATE_TYPES = [
    (SensorState, "sensor"),
    (BinarySensorState, "binary_sensor"),
    (SwitchState, "switch"),
    (TextSensorState, "text_sensor"),
    (LightState, "light"),
    (CoverState, "cover"),
    (FanState, "fan"),
    (LockEntityState, "lock"),
    (NumberState, "number"),
    (SelectState, "select"),
    (TextState, "text"),
    (ValveState, "valve"),
    (WaterHeaterState, "water_heater"),
    (AlarmControlPanelEntityState, "alarm_control_panel"),
    (DateState, "date"),
    (TimeState, "time"),
    (DateTimeState, "datetime"),
    # Event is stateless (no initial state sent on subscribe)
]


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
            if entity.name in ENTITY_TO_DEVICE:
                expected_device = ENTITY_TO_DEVICE[entity.name]
                entity_device_mapping[entity.key] = device_ids[expected_device]
            elif entity.name in NO_DEVICE_ENTITIES:
                entity_device_mapping[entity.key] = 0

        expected_count = len(ENTITY_TO_DEVICE) + len(NO_DEVICE_ENTITIES)
        assert len(entity_device_mapping) >= expected_count, (
            f"Expected at least {expected_count} mapped entities, "
            f"got {len(entity_device_mapping)}. "
            f"Missing: {set(ENTITY_TO_DEVICE) | NO_DEVICE_ENTITIES - {e.name for e in all_entities}}"
        )

        # Subscribe to states and wait for all mapped entities
        # Event entities are stateless (no initial state on subscribe),
        # so exclude them from the expected count
        stateless_keys = {e.key for e in all_entities if e.name == "Doorbell"}
        stateful_count = len(entity_device_mapping) - len(
            stateless_keys & entity_device_mapping.keys()
        )

        loop = asyncio.get_running_loop()
        states: dict[int, EntityState] = {}
        states_future: asyncio.Future[bool] = loop.create_future()

        def on_state(state: EntityState) -> None:
            if state.key in entity_device_mapping:
                states[state.key] = state
            if len(states) >= stateful_count and not states_future.done():
                states_future.set_result(True)

        client.subscribe_states(on_state)

        # Wait for states
        try:
            await asyncio.wait_for(states_future, timeout=10.0)
        except TimeoutError:
            received_names = {e.name for e in all_entities if e.key in states}
            missing_names = (
                (set(ENTITY_TO_DEVICE) | NO_DEVICE_ENTITIES)
                - received_names
                - {"Doorbell"}
            )
            pytest.fail(
                f"Did not receive all entity states within 10 seconds. "
                f"Received {len(states)} states. "
                f"Missing: {missing_names}"
            )

        # Verify each state has the correct device_id
        verified_count = 0
        for key, expected_device_id in entity_device_mapping.items():
            if key in states:
                state = states[key]
                entity_name = next(
                    (e.name for e in all_entities if e.key == key), f"key={key}"
                )

                assert state.device_id == expected_device_id, (
                    f"State for '{entity_name}' (type={type(state).__name__}) "
                    f"has device_id {state.device_id}, expected {expected_device_id}"
                )
                verified_count += 1

        # All stateful entities should be verified (everything except Doorbell event)
        expected_verified = expected_count - 1  # exclude Doorbell
        assert verified_count >= expected_verified, (
            f"Only verified {verified_count} states, expected at least {expected_verified}"
        )

        # Verify each expected state type has at least one instance with non-zero device_id
        for state_type, type_name in EXPECTED_STATE_TYPES:
            matching = [
                s
                for s in states.values()
                if isinstance(s, state_type) and s.device_id != 0
            ]
            assert matching, (
                f"No {type_name} state (type={state_type.__name__}) "
                f"with non-zero device_id found"
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
