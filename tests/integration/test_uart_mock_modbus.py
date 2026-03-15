"""Integration test for modbus component with virtual UART.

Tests:
test_uart_mock_modbus :
  1. Read a single register and parse successfully (basic_register)
  2. Read multiple registers from SDM meter and parse successfully (sdm_voltage), with some intermediate delay to simulate UART buffer time.

test_uart_mock_modbus_no_threshold :
  Test modbus with no rx_full_threshold set (simulating USB UART / non-hardware UART).
  Verifies the 50ms fallback timeout handles chunked data with USB packet gaps.

"""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import ButtonInfo, EntityState, NumberInfo, SensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping, find_entity
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
        "delayed_response": [],
        "late_response": [],
        "no_response": [],
        "exception_response": [],
    }

    basic_register_changed = loop.create_future()
    delayed_response_changed = loop.create_future()
    late_response_changed = loop.create_future()
    no_response_changed = loop.create_future()
    exception_response_changed = loop.create_future()

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
                elif (
                    sensor_name == "delayed_response"
                    and state.state == 255.0
                    and not delayed_response_changed.done()
                ):
                    delayed_response_changed.set_result(True)
                elif (
                    sensor_name == "late_response" and not late_response_changed.done()
                ):
                    late_response_changed.set_result(True)
                elif sensor_name == "no_response" and not no_response_changed.done():
                    no_response_changed.set_result(True)
                elif (
                    sensor_name == "exception_response"
                    and not exception_response_changed.done()
                ):
                    exception_response_changed.set_result(True)

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

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        try:
            await asyncio.wait_for(delayed_response_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for delayed_response change. Received sensor states:\n"
                f"  delayed_response: {sensor_states['delayed_response']}\n"
            )

        try:
            await asyncio.wait_for(late_response_changed, timeout=2.0)
            pytest.fail(
                f"late_response change should not have been triggered, but was. Received sensor states:\n"
                f"  late_response: {sensor_states['late_response']}\n"
            )
        except TimeoutError:
            pass  # Expected timeout since we never inject a response for late_response

        try:
            await asyncio.wait_for(no_response_changed, timeout=2.0)
            pytest.fail(
                f"no_response change should not have been triggered, but was. Received sensor states:\n"
                f"  no_response: {sensor_states['no_response']}\n"
            )
        except TimeoutError:
            pass  # Expected timeout since we never inject a response for no_response

        # Wait for basic register to be updated with successful parse
        try:
            await asyncio.wait_for(basic_register_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Basic Register change. Received sensor states:\n"
                f"  basic_register: {sensor_states['basic_register']}\n"
            )

        try:
            await asyncio.wait_for(exception_response_changed, timeout=2.0)
            pytest.fail(
                f"exception_response change should not have been triggered, but was. Received sensor states:\n"
                f"  exception_response: {sensor_states['exception_response']}\n"
            )
        except TimeoutError:
            pass


@pytest.mark.asyncio
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

    # Track error and warning logs
    error_log_lines: list[str] = []
    warning_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "[E][modbus" in line:
            error_log_lines.append(line)
        if "[W][modbus" in line:
            warning_log_lines.append(line)

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
        run_compiled(yaml_config, line_callback=line_callback),
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

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        # Wait for voltage to be updated with successful parse
        try:
            await asyncio.wait_for(voltage_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for SDM voltage change. Received sensor states:\n"
                f"  sdm_voltage: {sensor_states['sdm_voltage']}\n"
            )

        assert len(error_log_lines) == 0, (
            "Expect no errors logged by the modbus mock, but got:\n"
            + "\n".join(error_log_lines)
        )
        assert len(warning_log_lines) == 0, (
            "Expect no warnings logged by the modbus mock, but got:\n"
            + "\n".join(warning_log_lines)
        )


@pytest.mark.asyncio
async def test_uart_mock_modbus_no_threshold(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test modbus with no rx_full_threshold (simulating USB UART).

    Without the 50ms fallback timeout, the chunked response with a 40ms gap
    between USB packets would cause a false timeout and CRC failure cascade.
    """
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

    # Track error and warning logs
    error_log_lines: list[str] = []
    warning_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "[E][modbus" in line:
            error_log_lines.append(line)
        if "[W][modbus" in line:
            warning_log_lines.append(line)

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
        run_compiled(yaml_config, line_callback=line_callback),
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

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        # Wait for voltage to be updated with successful parse
        try:
            await asyncio.wait_for(voltage_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for SDM voltage change. Received sensor states:\n"
                f"  sdm_voltage: {sensor_states['sdm_voltage']}\n"
            )

        assert len(error_log_lines) == 0, (
            "Expect no errors logged by the modbus mock, but got:\n"
            + "\n".join(error_log_lines)
        )
        assert len(warning_log_lines) == 0, (
            "Expect no warnings logged by the modbus mock, but got:\n"
            + "\n".join(warning_log_lines)
        )


@pytest.mark.asyncio
@pytest.mark.xfail(
    reason="This test is currently expected to fail since the modbus parser cannot handle server responses from other devices. This will be implemented in a future PR."
)
async def test_uart_mock_modbus_server(
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
        "basic_read": [],
        "read_after_peer_response": [],
        "read_after_peer_timeout": [],
    }

    # Track error and warning logs
    error_log_lines: list[str] = []
    warning_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "[E][modbus" in line:
            error_log_lines.append(line)
        if "[W][modbus" in line:
            warning_log_lines.append(line)

    basic_read_changed = loop.create_future()
    read_after_peer_response_changed = loop.create_future()
    read_after_peer_timeout_changed = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in sensor_states:
                sensor_states[sensor_name].append(state.state)
                if (
                    sensor_name == "basic_read"
                    and state.state == 1
                    and not basic_read_changed.done()
                ):
                    basic_read_changed.set_result(True)
                elif (
                    sensor_name == "read_after_peer_response"
                    and state.state == 1
                    and not read_after_peer_response_changed.done()
                ):
                    read_after_peer_response_changed.set_result(True)
                elif (
                    sensor_name == "read_after_peer_timeout"
                    and state.state == 1
                    and not read_after_peer_timeout_changed.done()
                ):
                    read_after_peer_timeout_changed.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
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

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        try:
            await asyncio.wait_for(basic_read_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for basic_read change. Received sensor states:\n"
                f"  basic_read: {sensor_states['basic_read']}\n"
            )

        try:
            await asyncio.wait_for(read_after_peer_response_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for read_after_peer_response change. Received sensor states:\n"
                f"  read_after_peer_response: {sensor_states['read_after_peer_response']}\n"
            )

        try:
            await asyncio.wait_for(read_after_peer_timeout_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for read_after_peer_timeout change. Received sensor states:\n"
                f"  read_after_peer_timeout: {sensor_states['read_after_peer_timeout']}\n"
            )

        assert len(error_log_lines) == 0, (
            "Expect no errors logged by the modbus mock, but got:\n"
            + "\n".join(error_log_lines)
        )
        assert len(warning_log_lines) == 0, (
            "Expect no warnings logged by the modbus mock, but got:\n"
            + "\n".join(warning_log_lines)
        )


@pytest.mark.asyncio
async def test_uart_mock_modbus_server_controller(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test server/controller functionality for all read register types."""
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
        "reg_u_word": [],
        "reg_s_word": [],
        "reg_u_dword": [],
        "reg_s_dword": [],
        "reg_u_dword_r": [],
        "reg_s_dword_r": [],
        "reg_u_qword": [],
        "reg_s_qword": [],
        "reg_u_qword_r": [],
        "reg_s_qword_r": [],
        "reg_fp32": [],
        "reg_fp32_r": [],
    }

    # Track error and warning logs
    error_log_lines: list[str] = []
    warning_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "[E][modbus" in line:
            error_log_lines.append(line)
        if "[W][modbus" in line:
            warning_log_lines.append(line)

    reg_u_word_changed = loop.create_future()
    reg_s_word_changed = loop.create_future()
    reg_u_dword_changed = loop.create_future()
    reg_s_dword_changed = loop.create_future()
    reg_u_dword_r_changed = loop.create_future()
    reg_s_dword_r_changed = loop.create_future()
    reg_u_qword_changed = loop.create_future()
    reg_s_qword_changed = loop.create_future()
    reg_u_qword_r_changed = loop.create_future()
    reg_s_qword_r_changed = loop.create_future()
    reg_fp32_changed = loop.create_future()
    reg_fp32_r_changed = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in sensor_states:
                sensor_states[sensor_name].append(state.state)
                if (
                    sensor_name == "reg_u_word"
                    and state.state == 99
                    and not reg_u_word_changed.done()
                ):
                    reg_u_word_changed.set_result(True)
                elif (
                    sensor_name == "reg_s_word"
                    and state.state == -99
                    and not reg_s_word_changed.done()
                ):
                    reg_s_word_changed.set_result(True)
                elif (
                    sensor_name == "reg_u_dword"
                    and state.state == 16909060
                    and not reg_u_dword_changed.done()
                ):
                    reg_u_dword_changed.set_result(True)
                elif (
                    sensor_name == "reg_s_dword"
                    and state.state == -16909060
                    and not reg_s_dword_changed.done()
                ):
                    reg_s_dword_changed.set_result(True)
                elif (
                    sensor_name == "reg_u_dword_r"
                    and state.state == pytest.approx(67305985)
                    and not reg_u_dword_r_changed.done()
                ):
                    reg_u_dword_r_changed.set_result(True)
                elif (
                    sensor_name == "reg_s_dword_r"
                    and state.state == pytest.approx(-67305985)
                    and not reg_s_dword_r_changed.done()
                ):
                    reg_s_dword_r_changed.set_result(True)
                elif (
                    sensor_name == "reg_u_qword"
                    and state.state == pytest.approx(72623859790382856)
                    and not reg_u_qword_changed.done()
                ):
                    reg_u_qword_changed.set_result(True)
                elif (
                    sensor_name == "reg_s_qword"
                    and state.state == pytest.approx(-72623859790382856)
                    and not reg_s_qword_changed.done()
                ):
                    reg_s_qword_changed.set_result(True)
                elif (
                    sensor_name == "reg_u_qword_r"
                    and state.state == pytest.approx(578437695752307201)
                    and not reg_u_qword_r_changed.done()
                ):
                    reg_u_qword_r_changed.set_result(True)
                elif (
                    sensor_name == "reg_s_qword_r"
                    and state.state == pytest.approx(-578437695752307201)
                    and not reg_s_qword_r_changed.done()
                ):
                    reg_s_qword_r_changed.set_result(True)
                elif (
                    sensor_name == "reg_fp32"
                    and state.state == pytest.approx(3.14)
                    and not reg_fp32_changed.done()
                ):
                    reg_fp32_changed.set_result(True)
                elif (
                    sensor_name == "reg_fp32_r"
                    and state.state == pytest.approx(3.14)
                    and not reg_fp32_r_changed.done()
                ):
                    reg_fp32_r_changed.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
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

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        try:
            await asyncio.wait_for(reg_u_word_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_u_word change. Received sensor states:\n"
                f"  reg_u_word: {sensor_states['reg_u_word']}\n"
            )
        try:
            await asyncio.wait_for(reg_s_word_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_s_word change. Received sensor states:\n"
                f"  reg_s_word: {sensor_states['reg_s_word']}\n"
            )
        try:
            await asyncio.wait_for(reg_u_dword_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_u_dword change. Received sensor states:\n"
                f"  reg_u_dword: {sensor_states['reg_u_dword']}\n"
            )
        try:
            await asyncio.wait_for(reg_s_dword_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_s_dword change. Received sensor states:\n"
                f"  reg_s_dword: {sensor_states['reg_s_dword']}\n"
            )
        try:
            await asyncio.wait_for(reg_u_dword_r_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_u_dword_r change. Received sensor states:\n"
                f"  reg_u_dword_r: {sensor_states['reg_u_dword_r']}\n"
            )
        try:
            await asyncio.wait_for(reg_s_dword_r_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_s_dword_r change. Received sensor states:\n"
                f"  reg_s_dword_r: {sensor_states['reg_s_dword_r']}\n"
            )
        try:
            await asyncio.wait_for(reg_u_qword_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_u_qword change. Received sensor states:\n"
                f"  reg_u_qword: {sensor_states['reg_u_qword']}\n"
            )
        try:
            await asyncio.wait_for(reg_s_qword_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_s_qword change. Received sensor states:\n"
                f"  reg_s_qword: {sensor_states['reg_s_qword']}\n"
            )
        try:
            await asyncio.wait_for(reg_u_qword_r_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_u_qword_r change. Received sensor states:\n"
                f"  reg_u_qword_r: {sensor_states['reg_u_qword_r']}\n"
            )
        try:
            await asyncio.wait_for(reg_s_qword_r_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_s_qword_r change. Received sensor states:\n"
                f"  reg_s_qword_r: {sensor_states['reg_s_qword_r']}\n"
            )
        try:
            await asyncio.wait_for(reg_fp32_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_fp32 change. Received sensor states:\n"
                f"  reg_fp32: {sensor_states['reg_fp32']}\n"
            )
        try:
            await asyncio.wait_for(reg_fp32_r_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_fp32_r change. Received sensor states:\n"
                f"  reg_fp32_r: {sensor_states['reg_fp32_r']}\n"
            )

        assert len(error_log_lines) == 0, (
            "Expect no errors logged by the modbus mock, but got:\n"
            + "\n".join(error_log_lines)
        )
        assert len(warning_log_lines) == 0, (
            "Expect no warnings logged by the modbus mock, but got:\n"
            + "\n".join(warning_log_lines)
        )


@pytest.mark.asyncio
async def test_uart_mock_modbus_server_controller_write(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test server/controller write functionality for all register value types.

    Verifies that writing to modbus server registers via the controller updates
    the server's stored values, which are then read back correctly on the next poll.
    All 12 value types are tested: U/S_WORD, U/S_DWORD(_R), U/S_QWORD(_R), FP32(_R).
    """
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
        "reg_u_word": [],
        "reg_s_word": [],
        "reg_u_dword": [],
        "reg_s_dword": [],
        "reg_u_dword_r": [],
        "reg_s_dword_r": [],
        "reg_u_qword": [],
        "reg_s_qword": [],
        "reg_u_qword_r": [],
        "reg_s_qword_r": [],
        "reg_fp32": [],
        "reg_fp32_r": [],
    }

    # Track error and warning logs
    error_log_lines: list[str] = []
    warning_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "[E][modbus" in line:
            error_log_lines.append(line)
        if "[W][modbus" in line:
            warning_log_lines.append(line)

    # Futures for initial baseline reads (confirm UART connection is working)
    reg_u_word_initial = loop.create_future()
    reg_s_word_initial = loop.create_future()
    reg_u_dword_initial = loop.create_future()
    reg_s_dword_initial = loop.create_future()
    reg_u_dword_r_initial = loop.create_future()
    reg_s_dword_r_initial = loop.create_future()
    reg_u_qword_initial = loop.create_future()
    reg_s_qword_initial = loop.create_future()
    reg_u_qword_r_initial = loop.create_future()
    reg_s_qword_r_initial = loop.create_future()
    reg_fp32_initial = loop.create_future()
    reg_fp32_r_initial = loop.create_future()

    # Futures for post-write reads (confirm values were stored on the server)
    reg_u_word_written = loop.create_future()
    reg_s_word_written = loop.create_future()
    reg_u_dword_written = loop.create_future()
    reg_s_dword_written = loop.create_future()
    reg_u_dword_r_written = loop.create_future()
    reg_s_dword_r_written = loop.create_future()
    reg_u_qword_written = loop.create_future()
    reg_s_qword_written = loop.create_future()
    reg_u_qword_r_written = loop.create_future()
    reg_s_qword_r_written = loop.create_future()
    reg_fp32_written = loop.create_future()
    reg_fp32_r_written = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in sensor_states:
                sensor_states[sensor_name].append(state.state)
                if sensor_name == "reg_u_word":
                    if state.state == 11 and not reg_u_word_initial.done():
                        reg_u_word_initial.set_result(True)
                    elif state.state == 42 and not reg_u_word_written.done():
                        reg_u_word_written.set_result(True)
                elif sensor_name == "reg_s_word":
                    if state.state == -11 and not reg_s_word_initial.done():
                        reg_s_word_initial.set_result(True)
                    elif state.state == -42 and not reg_s_word_written.done():
                        reg_s_word_written.set_result(True)
                elif sensor_name == "reg_u_dword":
                    if state.state == 1001 and not reg_u_dword_initial.done():
                        reg_u_dword_initial.set_result(True)
                    elif state.state == 2002 and not reg_u_dword_written.done():
                        reg_u_dword_written.set_result(True)
                elif sensor_name == "reg_s_dword":
                    if state.state == -1001 and not reg_s_dword_initial.done():
                        reg_s_dword_initial.set_result(True)
                    elif state.state == -2002 and not reg_s_dword_written.done():
                        reg_s_dword_written.set_result(True)
                elif sensor_name == "reg_u_dword_r":
                    if state.state == 3003 and not reg_u_dword_r_initial.done():
                        reg_u_dword_r_initial.set_result(True)
                    elif state.state == 4004 and not reg_u_dword_r_written.done():
                        reg_u_dword_r_written.set_result(True)
                elif sensor_name == "reg_s_dword_r":
                    if state.state == -3003 and not reg_s_dword_r_initial.done():
                        reg_s_dword_r_initial.set_result(True)
                    elif state.state == -4004 and not reg_s_dword_r_written.done():
                        reg_s_dword_r_written.set_result(True)
                elif sensor_name == "reg_u_qword":
                    if state.state == 5005 and not reg_u_qword_initial.done():
                        reg_u_qword_initial.set_result(True)
                    elif state.state == 6006 and not reg_u_qword_written.done():
                        reg_u_qword_written.set_result(True)
                elif sensor_name == "reg_s_qword":
                    if state.state == -5005 and not reg_s_qword_initial.done():
                        reg_s_qword_initial.set_result(True)
                    elif state.state == -6006 and not reg_s_qword_written.done():
                        reg_s_qword_written.set_result(True)
                elif sensor_name == "reg_u_qword_r":
                    if state.state == 7007 and not reg_u_qword_r_initial.done():
                        reg_u_qword_r_initial.set_result(True)
                    elif state.state == 8008 and not reg_u_qword_r_written.done():
                        reg_u_qword_r_written.set_result(True)
                elif sensor_name == "reg_s_qword_r":
                    if state.state == -7007 and not reg_s_qword_r_initial.done():
                        reg_s_qword_r_initial.set_result(True)
                    elif state.state == -8008 and not reg_s_qword_r_written.done():
                        reg_s_qword_r_written.set_result(True)
                elif sensor_name == "reg_fp32":
                    if (
                        state.state == pytest.approx(1.5, abs=0.01)
                        and not reg_fp32_initial.done()
                    ):
                        reg_fp32_initial.set_result(True)
                    elif (
                        state.state == pytest.approx(3.14, abs=0.01)
                        and not reg_fp32_written.done()
                    ):
                        reg_fp32_written.set_result(True)
                elif sensor_name == "reg_fp32_r":
                    if (
                        state.state == pytest.approx(2.5, abs=0.01)
                        and not reg_fp32_r_initial.done()
                    ):
                        reg_fp32_r_initial.set_result(True)
                    elif (
                        state.state == pytest.approx(6.28, abs=0.01)
                        and not reg_fp32_r_written.done()
                    ):
                        reg_fp32_r_written.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
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

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        # Wait for initial baseline values to confirm the controller <-> server
        # connection is working before issuing writes
        try:
            await asyncio.wait_for(
                asyncio.gather(
                    reg_u_word_initial,
                    reg_s_word_initial,
                    reg_u_dword_initial,
                    reg_s_dword_initial,
                    reg_u_dword_r_initial,
                    reg_s_dword_r_initial,
                    reg_u_qword_initial,
                    reg_s_qword_initial,
                    reg_u_qword_r_initial,
                    reg_s_qword_r_initial,
                    reg_fp32_initial,
                    reg_fp32_r_initial,
                ),
                timeout=4.0,
            )
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for initial sensor reads. Received sensor states:\n"
                + "\n".join(f"  {k}: {v}" for k, v in sensor_states.items())
            )

        # Find all number entities for issuing write commands
        write_u_word = find_entity(entities, "write_u_word", NumberInfo)
        write_s_word = find_entity(entities, "write_s_word", NumberInfo)
        write_u_dword = find_entity(entities, "write_u_dword", NumberInfo)
        write_s_dword = find_entity(entities, "write_s_dword", NumberInfo)
        write_u_dword_r = find_entity(entities, "write_u_dword_r", NumberInfo)
        write_s_dword_r = find_entity(entities, "write_s_dword_r", NumberInfo)
        write_u_qword = find_entity(entities, "write_u_qword", NumberInfo)
        write_s_qword = find_entity(entities, "write_s_qword", NumberInfo)
        write_u_qword_r = find_entity(entities, "write_u_qword_r", NumberInfo)
        write_s_qword_r = find_entity(entities, "write_s_qword_r", NumberInfo)
        write_fp32 = find_entity(entities, "write_fp32", NumberInfo)
        write_fp32_r = find_entity(entities, "write_fp32_r", NumberInfo)
        assert write_u_word is not None, "write_u_word number entity not found"
        assert write_s_word is not None, "write_s_word number entity not found"
        assert write_u_dword is not None, "write_u_dword number entity not found"
        assert write_s_dword is not None, "write_s_dword number entity not found"
        assert write_u_dword_r is not None, "write_u_dword_r number entity not found"
        assert write_s_dword_r is not None, "write_s_dword_r number entity not found"
        assert write_u_qword is not None, "write_u_qword number entity not found"
        assert write_s_qword is not None, "write_s_qword number entity not found"
        assert write_u_qword_r is not None, "write_u_qword_r number entity not found"
        assert write_s_qword_r is not None, "write_s_qword_r number entity not found"
        assert write_fp32 is not None, "write_fp32 number entity not found"
        assert write_fp32_r is not None, "write_fp32_r number entity not found"

        client.number_command(write_u_word.key, 42)
        client.number_command(write_s_word.key, -42)
        client.number_command(write_u_dword.key, 2002)
        client.number_command(write_s_dword.key, -2002)
        client.number_command(write_u_dword_r.key, 4004)
        client.number_command(write_s_dword_r.key, -4004)
        client.number_command(write_u_qword.key, 6006)
        client.number_command(write_s_qword.key, -6006)
        client.number_command(write_u_qword_r.key, 8008)
        client.number_command(write_s_qword_r.key, -8008)
        client.number_command(write_fp32.key, 3.14)
        client.number_command(write_fp32_r.key, 6.28)

        # Wait for sensors to reflect the written values (confirmed round-trip write+read)
        for future, name in [
            (reg_u_word_written, "reg_u_word"),
            (reg_s_word_written, "reg_s_word"),
            (reg_u_dword_written, "reg_u_dword"),
            (reg_s_dword_written, "reg_s_dword"),
            (reg_u_dword_r_written, "reg_u_dword_r"),
            (reg_s_dword_r_written, "reg_s_dword_r"),
            (reg_u_qword_written, "reg_u_qword"),
            (reg_s_qword_written, "reg_s_qword"),
            (reg_u_qword_r_written, "reg_u_qword_r"),
            (reg_s_qword_r_written, "reg_s_qword_r"),
            (reg_fp32_written, "reg_fp32"),
            (reg_fp32_r_written, "reg_fp32_r"),
        ]:
            try:
                await asyncio.wait_for(future, timeout=4.0)
            except TimeoutError:
                pytest.fail(
                    f"Timeout waiting for {name} write confirmation. "
                    f"Received sensor states: {sensor_states[name]}"
                )

        assert len(error_log_lines) == 0, (
            "Expect no errors logged by the modbus mock, but got:\n"
            + "\n".join(error_log_lines)
        )
        assert len(warning_log_lines) == 0, (
            "Expect no warnings logged by the modbus mock, but got:\n"
            + "\n".join(warning_log_lines)
        )


@pytest.mark.asyncio
@pytest.mark.xfail(
    reason="This test is currently expected to fail since the modbus parser cannot handle server responses from other devices. This will be implemented in a future PR."
)
async def test_uart_mock_modbus_server_controller_multiple(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test server/controller functionality with multiple servers."""
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
        "reg_u_word": [],
        "reg_u_word_2": [],
    }

    # Track error and warning logs
    error_log_lines: list[str] = []
    warning_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "[E][modbus" in line:
            error_log_lines.append(line)
        if "[W][modbus" in line:
            warning_log_lines.append(line)

    reg_u_word_changed = loop.create_future()
    reg_u_word_2_changed = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, SensorState) and not state.missing_state:
            sensor_name = key_to_sensor.get(state.key)
            if sensor_name and sensor_name in sensor_states:
                sensor_states[sensor_name].append(state.state)
                if (
                    sensor_name == "reg_u_word"
                    and state.state == 919
                    and not reg_u_word_changed.done()
                ):
                    reg_u_word_changed.set_result(True)
                elif (
                    sensor_name == "reg_u_word_2"
                    and state.state == 929
                    and not reg_u_word_2_changed.done()
                ):
                    reg_u_word_2_changed.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
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

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        try:
            await asyncio.wait_for(reg_u_word_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_u_word change. Received sensor states:\n"
                f"  reg_u_word: {sensor_states['reg_u_word']}\n"
            )
        try:
            await asyncio.wait_for(reg_u_word_2_changed, timeout=2.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for reg_u_word_2 change. Received sensor states:\n"
                f"  reg_u_word_2: {sensor_states['reg_u_word_2']}\n"
            )

        assert len(error_log_lines) == 0, (
            "Expect no errors logged by the modbus mock, but got:\n"
            + "\n".join(error_log_lines)
        )
        assert len(warning_log_lines) == 0, (
            "Expect no warnings logged by the modbus mock, but got:\n"
            + "\n".join(warning_log_lines)
        )
