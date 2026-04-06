"""Integration test for template alarm control panel with many sensors."""

from __future__ import annotations

import aioesphomeapi
from aioesphomeapi.model import APIIntEnum
import pytest

from .state_utils import InitialStateHelper
from .types import APIClientConnectedFactory, RunCompiledFunction


class EspHomeACPFeatures(APIIntEnum):
    """ESPHome AlarmControlPanel feature numbers."""

    ARM_HOME = 1
    ARM_AWAY = 2
    ARM_NIGHT = 4
    TRIGGER = 8
    ARM_CUSTOM_BYPASS = 16
    ARM_VACATION = 32


@pytest.mark.asyncio
async def test_template_alarm_control_panel_many_sensors(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test template alarm control panel with 10 binary sensors using FixedVector."""
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Get entity info first
        entities, _ = await client.list_entities_services()

        # Find the alarm control panel and binary sensors
        alarm_info: aioesphomeapi.AlarmControlPanelInfo | None = None
        binary_sensors: list[aioesphomeapi.BinarySensorInfo] = []

        for entity in entities:
            if isinstance(entity, aioesphomeapi.AlarmControlPanelInfo):
                alarm_info = entity
            elif isinstance(entity, aioesphomeapi.BinarySensorInfo):
                binary_sensors.append(entity)

        assert alarm_info is not None, "Alarm control panel entity info not found"
        assert alarm_info.name == "Test Alarm"
        assert alarm_info.requires_code is True
        assert alarm_info.requires_code_to_arm is True

        # Verify we have 10 binary sensors
        assert len(binary_sensors) == 10, (
            f"Expected 10 binary sensors, got {len(binary_sensors)}"
        )

        # Verify sensor names
        expected_sensor_names = {
            "Door 1",
            "Door 2",
            "Window 1",
            "Window 2",
            "Motion 1",
            "Motion 2",
            "Glass Break 1",
            "Glass Break 2",
            "Smoke Detector",
            "CO Detector",
        }
        actual_sensor_names = {sensor.name for sensor in binary_sensors}
        assert actual_sensor_names == expected_sensor_names, (
            f"Sensor names mismatch. Expected: {expected_sensor_names}, "
            f"Got: {actual_sensor_names}"
        )

        # Use InitialStateHelper to wait for all initial states
        state_helper = InitialStateHelper(entities)

        def on_state(state: aioesphomeapi.EntityState) -> None:
            # We'll receive subsequent states here after initial states
            pass

        client.subscribe_states(state_helper.on_state_wrapper(on_state))

        # Wait for all initial states
        await state_helper.wait_for_initial_states(timeout=5.0)

        # Verify the alarm state is disarmed initially
        alarm_state = state_helper.initial_states.get(alarm_info.key)
        assert alarm_state is not None, "Alarm control panel initial state not received"
        assert isinstance(alarm_state, aioesphomeapi.AlarmControlPanelEntityState)
        assert alarm_state.state == aioesphomeapi.AlarmControlPanelState.DISARMED, (
            f"Expected initial state DISARMED, got {alarm_state.state}"
        )

        # Verify all 10 binary sensors have initial states
        binary_sensor_states = [
            state_helper.initial_states.get(sensor.key) for sensor in binary_sensors
        ]
        assert all(state is not None for state in binary_sensor_states), (
            "Not all binary sensors have initial states"
        )

        # Verify all binary sensor states are BinarySensorState type
        for i, state in enumerate(binary_sensor_states):
            assert isinstance(state, aioesphomeapi.BinarySensorState), (
                f"Binary sensor {i} state is not BinarySensorState: {type(state)}"
            )

        # Verify supported features
        expected_features = (
            EspHomeACPFeatures.ARM_HOME
            | EspHomeACPFeatures.ARM_AWAY
            | EspHomeACPFeatures.ARM_NIGHT
            | EspHomeACPFeatures.TRIGGER
        )
        assert alarm_info.supported_features == expected_features, (
            f"Expected supported_features={expected_features} (ARM_HOME|ARM_AWAY|ARM_NIGHT|TRIGGER), "
            f"got {alarm_info.supported_features}"
        )
