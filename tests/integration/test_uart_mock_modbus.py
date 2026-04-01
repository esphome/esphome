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
from collections.abc import Callable
from pathlib import Path

from aioesphomeapi import ButtonInfo, EntityState, NumberInfo, SensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping, find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _replace_external_components_path(yaml_config: str) -> str:
    external_components_path = str(
        Path(__file__).parent / "fixtures" / "external_components"
    )
    return yaml_config.replace("EXTERNAL_COMPONENT_PATH", external_components_path)


def _make_modbus_line_callback() -> tuple[Callable[[str], None], list[str], list[str]]:
    """Return a (callback, error_lines, warning_lines) tuple for tracking modbus log output."""
    error_log_lines: list[str] = []
    warning_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "[E][modbus" in line:
            error_log_lines.append(line)
        if "[W][modbus" in line:
            warning_log_lines.append(line)

    return line_callback, error_log_lines, warning_log_lines


def _assert_no_modbus_errors(
    error_log_lines: list[str], warning_log_lines: list[str]
) -> None:
    assert len(error_log_lines) == 0, (
        "Expect no errors logged by the modbus mock, but got:\n"
        + "\n".join(error_log_lines)
    )
    assert len(warning_log_lines) == 0, (
        "Expect no warnings logged by the modbus mock, but got:\n"
        + "\n".join(warning_log_lines)
    )


async def _await_sensor_change(
    future: asyncio.Future,
    name: str,
    sensor_states: dict[str, list[float]],
    timeout: float = 2.0,
) -> None:
    """Wait for a sensor future to resolve; fail the test on timeout."""
    try:
        await asyncio.wait_for(future, timeout=timeout)
    except TimeoutError:
        pytest.fail(
            f"Timeout waiting for {name} change. Received sensor states:\n"
            f"  {name}: {sensor_states[name]}\n"
        )


async def _await_sensor_must_not_change(
    future: asyncio.Future,
    name: str,
    sensor_states: dict[str, list[float]],
    timeout: float = 2.0,
) -> None:
    """Assert a sensor future does NOT resolve within the timeout."""
    try:
        await asyncio.wait_for(future, timeout=timeout)
        pytest.fail(
            f"{name} change should not have been triggered, but was. "
            f"Received sensor states:\n  {name}: {sensor_states[name]}\n"
        )
    except TimeoutError:
        pass  # Expected


async def _setup_and_start_scenario(
    client,
    sensor_states: dict[str, list[float]],
    on_state: Callable,
    key_to_sensor: dict,
) -> list:
    """Wire up entity subscriptions, wait for initial states, then press Start Scenario.

    Populates *key_to_sensor* in-place and returns the entity list.
    """
    entities, _ = await client.list_entities_services()
    key_to_sensor.update(
        build_key_to_entity_mapping(entities, list(sensor_states.keys()))
    )
    initial_state_helper = InitialStateHelper(entities)
    client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
    try:
        await initial_state_helper.wait_for_initial_states()
    except TimeoutError:
        pytest.fail("Timeout waiting for initial states")
    start_btn = find_entity(entities, "start_scenario", ButtonInfo)
    assert start_btn is not None, "Start Scenario button not found"
    client.button_command(start_btn.key)
    return entities


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


@pytest.mark.asyncio
async def test_uart_mock_modbus(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test basic modbus data parsing."""
    yaml_config = _replace_external_components_path(yaml_config)
    loop = asyncio.get_running_loop()

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

    key_to_sensor: dict[int, str] = {}

    def on_state(state: EntityState) -> None:
        if not isinstance(state, SensorState) or state.missing_state:
            return
        sensor_name = key_to_sensor.get(state.key)
        if not sensor_name or sensor_name not in sensor_states:
            return
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
        elif sensor_name == "late_response" and not late_response_changed.done():
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
        await _setup_and_start_scenario(client, sensor_states, on_state, key_to_sensor)

        await _await_sensor_change(
            delayed_response_changed, "delayed_response", sensor_states
        )
        await _await_sensor_must_not_change(
            late_response_changed, "late_response", sensor_states
        )
        await _await_sensor_must_not_change(
            no_response_changed, "no_response", sensor_states
        )
        await _await_sensor_change(
            basic_register_changed, "basic_register", sensor_states
        )
        await _await_sensor_must_not_change(
            exception_response_changed, "exception_response", sensor_states
        )


@pytest.mark.asyncio
async def test_uart_mock_modbus_timing(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test basic modbus data parsing."""
    yaml_config = _replace_external_components_path(yaml_config)
    loop = asyncio.get_running_loop()

    sensor_states: dict[str, list[float]] = {"sdm_voltage": []}
    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()
    voltage_changed = loop.create_future()
    key_to_sensor: dict[int, str] = {}

    def on_state(state: EntityState) -> None:
        if not isinstance(state, SensorState) or state.missing_state:
            return
        sensor_name = key_to_sensor.get(state.key)
        if not sensor_name or sensor_name not in sensor_states:
            return
        sensor_states[sensor_name].append(state.state)
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
        await _setup_and_start_scenario(client, sensor_states, on_state, key_to_sensor)
        await _await_sensor_change(voltage_changed, "sdm_voltage", sensor_states)
        _assert_no_modbus_errors(error_log_lines, warning_log_lines)


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
    yaml_config = _replace_external_components_path(yaml_config)
    loop = asyncio.get_running_loop()

    sensor_states: dict[str, list[float]] = {"sdm_voltage": []}
    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()
    voltage_changed = loop.create_future()
    key_to_sensor: dict[int, str] = {}

    def on_state(state: EntityState) -> None:
        if not isinstance(state, SensorState) or state.missing_state:
            return
        sensor_name = key_to_sensor.get(state.key)
        if not sensor_name or sensor_name not in sensor_states:
            return
        sensor_states[sensor_name].append(state.state)
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
        await _setup_and_start_scenario(client, sensor_states, on_state, key_to_sensor)
        await _await_sensor_change(voltage_changed, "sdm_voltage", sensor_states)
        _assert_no_modbus_errors(error_log_lines, warning_log_lines)


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
    yaml_config = _replace_external_components_path(yaml_config)
    loop = asyncio.get_running_loop()

    sensor_states: dict[str, list[float]] = {
        "basic_read": [],
        "read_after_peer_response": [],
        "read_after_peer_timeout": [],
    }
    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    basic_read_changed = loop.create_future()
    read_after_peer_response_changed = loop.create_future()
    read_after_peer_timeout_changed = loop.create_future()

    key_to_sensor: dict[int, str] = {}

    def on_state(state: EntityState) -> None:
        if not isinstance(state, SensorState) or state.missing_state:
            return
        sensor_name = key_to_sensor.get(state.key)
        if not sensor_name or sensor_name not in sensor_states:
            return
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
        await _setup_and_start_scenario(client, sensor_states, on_state, key_to_sensor)
        await _await_sensor_change(basic_read_changed, "basic_read", sensor_states)
        await _await_sensor_change(
            read_after_peer_response_changed, "read_after_peer_response", sensor_states
        )
        await _await_sensor_change(
            read_after_peer_timeout_changed, "read_after_peer_timeout", sensor_states
        )
        _assert_no_modbus_errors(error_log_lines, warning_log_lines)


@pytest.mark.asyncio
async def test_uart_mock_modbus_server_controller(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test server/controller functionality for all read register types."""
    yaml_config = _replace_external_components_path(yaml_config)
    loop = asyncio.get_running_loop()

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
    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

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

    key_to_sensor: dict[int, str] = {}

    def on_state(state: EntityState) -> None:
        if not isinstance(state, SensorState) or state.missing_state:
            return
        sensor_name = key_to_sensor.get(state.key)
        if not sensor_name or sensor_name not in sensor_states:
            return
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
        await _setup_and_start_scenario(client, sensor_states, on_state, key_to_sensor)

        for future, name in [
            (reg_u_word_changed, "reg_u_word"),
            (reg_s_word_changed, "reg_s_word"),
            (reg_u_dword_changed, "reg_u_dword"),
            (reg_s_dword_changed, "reg_s_dword"),
            (reg_u_dword_r_changed, "reg_u_dword_r"),
            (reg_s_dword_r_changed, "reg_s_dword_r"),
            (reg_u_qword_changed, "reg_u_qword"),
            (reg_s_qword_changed, "reg_s_qword"),
            (reg_u_qword_r_changed, "reg_u_qword_r"),
            (reg_s_qword_r_changed, "reg_s_qword_r"),
            (reg_fp32_changed, "reg_fp32"),
            (reg_fp32_r_changed, "reg_fp32_r"),
        ]:
            await _await_sensor_change(future, name, sensor_states)

        _assert_no_modbus_errors(error_log_lines, warning_log_lines)


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
    yaml_config = _replace_external_components_path(yaml_config)
    loop = asyncio.get_running_loop()

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
    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

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

    key_to_sensor: dict[int, str] = {}

    def on_state(state: EntityState) -> None:
        if not isinstance(state, SensorState) or state.missing_state:
            return
        sensor_name = key_to_sensor.get(state.key)
        if not sensor_name or sensor_name not in sensor_states:
            return
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
        entities = await _setup_and_start_scenario(
            client, sensor_states, on_state, key_to_sensor
        )

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

        # Find all number entities and issue write commands
        for name, value in [
            ("write_u_word", 42),
            ("write_s_word", -42),
            ("write_u_dword", 2002),
            ("write_s_dword", -2002),
            ("write_u_dword_r", 4004),
            ("write_s_dword_r", -4004),
            ("write_u_qword", 6006),
            ("write_s_qword", -6006),
            ("write_u_qword_r", 8008),
            ("write_s_qword_r", -8008),
            ("write_fp32", 3.14),
            ("write_fp32_r", 6.28),
        ]:
            entity = find_entity(entities, name, NumberInfo)
            assert entity is not None, f"{name} number entity not found"
            client.number_command(entity.key, value)

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
            await _await_sensor_change(future, name, sensor_states, timeout=4.0)

        _assert_no_modbus_errors(error_log_lines, warning_log_lines)


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
    yaml_config = _replace_external_components_path(yaml_config)
    loop = asyncio.get_running_loop()

    sensor_states: dict[str, list[float]] = {
        "reg_u_word": [],
        "reg_u_word_2": [],
    }
    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    reg_u_word_changed = loop.create_future()
    reg_u_word_2_changed = loop.create_future()

    key_to_sensor: dict[int, str] = {}

    def on_state(state: EntityState) -> None:
        if not isinstance(state, SensorState) or state.missing_state:
            return
        sensor_name = key_to_sensor.get(state.key)
        if not sensor_name or sensor_name not in sensor_states:
            return
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
        await _setup_and_start_scenario(client, sensor_states, on_state, key_to_sensor)
        await _await_sensor_change(reg_u_word_changed, "reg_u_word", sensor_states)
        await _await_sensor_change(reg_u_word_2_changed, "reg_u_word_2", sensor_states)
        _assert_no_modbus_errors(error_log_lines, warning_log_lines)
