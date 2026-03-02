"""Integration test for modbus component with virtual UART.

Tests:
test_uart_mock_modbus :
  1. Read a single register and parse successfully (basic_register)
  2. Read multiple registers from SDM meter and parse successfully (sdm_voltage), with some intermediate delay to simulate UART buffer time.

"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import EntityState, SensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_modbus(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test basic modbus data parsing."""
    # Replace external component path placeholder
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()

    # Track sensor state updates (after initial state is swallowed)
    sensor_states: dict[str, list[float]] = {
        "basic_register": [],
    }

    basic_register_changed = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in sensor_states:
                sensor_states[sensor_name].append(state.state)
                if (
                    sensor_name == "basic_register"
                    and state.state == 259.0
                    and not basic_register_changed.done()
                ):
                    basic_register_changed.set_result(True)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        # Build key mappings for all sensor types
        all_names = list(sensor_states.keys())
        key_to_sensor = build_key_to_entity_mapping(entities, all_names)

        # Set up initial state helper
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Wait for basic register to be updated with successful parse
        try:
            await asyncio.wait_for(basic_register_changed, timeout=15.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Basic Register change. Received sensor states:\n"
                f"  basic_register: {sensor_states['basic_register']}\n"
            )


@pytest.mark.asyncio
@pytest.mark.xfail(
    reason="There is a bug in UART which will timeout for long responses."
)
async def test_uart_mock_modbus_timing(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test basic modbus data parsing."""
    # Replace external component path placeholder
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH", external_components_path
    )

    loop = asyncio.get_running_loop()

    # Track sensor state updates (after initial state is swallowed)
    sensor_states: dict[str, list[float]] = {
        "sdm_voltage": [],
    }

    voltage_changed = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in sensor_states:
                sensor_states[sensor_name].append(state.state)
                # Check if this is a good voltage reading (243V)
                if (
                    sensor_name == "sdm_voltage"
                    and state.state > 200.0
                    and not voltage_changed.done()
                ):
                    voltage_changed.set_result(True)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        # Build key mappings for all sensor types
        all_names = list(sensor_states.keys())
        key_to_sensor = build_key_to_entity_mapping(entities, all_names)

        # Set up initial state helper
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))

        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        # Wait for voltage to be updated with successful parse
        try:
            await asyncio.wait_for(voltage_changed, timeout=15.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for SDM voltage change. Received sensor states:\n"
                f"  sdm_voltage: {sensor_states['sdm_voltage']}\n"
            )
