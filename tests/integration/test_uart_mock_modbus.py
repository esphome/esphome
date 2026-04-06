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
from dataclasses import dataclass

from aioesphomeapi import NumberInfo
import pytest

from .state_utils import SensorTracker, find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@dataclass
class RegisterTestCase:
    """Test parameters for a single modbus register write/read round-trip."""

    initial_value: object
    write_number_name: str
    write_value: float
    post_write_value: object


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_modbus_line_callback() -> tuple[Callable[[str], None], list[str], list[str]]:
    """Return a (callback, error_lines, warning_lines) tuple for tracking modbus log output.

    Only captures bus-level modbus messages ([modbus:]), not modbus_controller
    scheduling noise (e.g. "Duplicate modbus command found").
    """
    error_log_lines: list[str] = []
    warning_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "[E][modbus:" in line:
            error_log_lines.append(line)
        if "[W][modbus:" in line:
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

    tracker = SensorTracker(
        [
            "basic_register",
            "delayed_response",
            "late_response",
            "no_response",
            "exception_response",
        ]
    )
    basic_register_changed = tracker.expect("basic_register", 259.0)
    delayed_response_changed = tracker.expect("delayed_response", 255.0)
    # late_response / no_response / exception_response: expect *any* value
    # (these should never fire, so we use a permissive match via expect_any)
    late_response_changed = tracker.expect_any("late_response")
    no_response_changed = tracker.expect_any("no_response")
    exception_response_changed = tracker.expect_any("exception_response")

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)

        await tracker.await_change(delayed_response_changed, "delayed_response")
        await tracker.await_change(basic_register_changed, "basic_register")
        # Run all "must not change" checks concurrently — each waits the full
        # timeout, so sequential execution would multiply the wall time.
        await asyncio.gather(
            tracker.await_must_not_change(late_response_changed, "late_response"),
            tracker.await_must_not_change(no_response_changed, "no_response"),
            tracker.await_must_not_change(
                exception_response_changed, "exception_response"
            ),
        )


@pytest.mark.asyncio
async def test_uart_mock_modbus_timing(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test modbus timing with multi-register SDM meter response."""

    tracker = SensorTracker(["sdm_voltage"])
    voltage_changed = tracker.expect_any("sdm_voltage")

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        await tracker.await_change(voltage_changed, "sdm_voltage")


@pytest.mark.asyncio
async def test_uart_mock_modbus_no_threshold(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test modbus with no rx_full_threshold (simulating USB UART).

    Without the 50ms fallback timeout, the chunked response with a 40ms gap
    between USB packets would cause a false timeout and CRC failure cascade.
    Bus-level warnings (CRC failures, buffer clears) are expected during
    chunked reassembly — the test only verifies the final value arrives.
    """

    tracker = SensorTracker(["sdm_voltage"])
    voltage_changed = tracker.expect_any("sdm_voltage")

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        await tracker.await_change(voltage_changed, "sdm_voltage")


@pytest.mark.asyncio
@pytest.mark.xfail(
    reason="Modbus parser cannot handle server responses from other devices on the bus. Fix tracked in PR #11969.",
    strict=True,
)
async def test_uart_mock_modbus_server(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test modbus server parsing with peer traffic on a shared bus."""

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    tracker = SensorTracker(
        ["basic_read", "read_after_peer_response", "read_after_peer_timeout"]
    )
    futures = tracker.expect_all(
        {
            "basic_read": 1,
            "read_after_peer_response": 1,
            "read_after_peer_timeout": 1,
        }
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        await tracker.await_all(futures)
        _assert_no_modbus_errors(error_log_lines, warning_log_lines)


@pytest.mark.asyncio
async def test_uart_mock_modbus_server_controller(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test server/controller functionality for all read register types."""

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    expected_values = {
        "reg_u_word": 99,
        "reg_s_word": -99,
        "reg_u_dword": 16909060,
        "reg_s_dword": -16909060,
        "reg_u_dword_r": pytest.approx(67305985),
        "reg_s_dword_r": pytest.approx(-67305985),
        "reg_u_qword": pytest.approx(72623859790382856),
        "reg_s_qword": pytest.approx(-72623859790382856),
        "reg_u_qword_r": pytest.approx(578437695752307201),
        "reg_s_qword_r": pytest.approx(-578437695752307201),
        "reg_fp32": pytest.approx(3.14),
        "reg_fp32_r": pytest.approx(3.14),
    }
    tracker = SensorTracker(list(expected_values.keys()))
    futures = tracker.expect_all(expected_values)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        await tracker.await_all(futures)
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

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    register_test_cases: dict[str, RegisterTestCase] = {
        "reg_u_word": RegisterTestCase(11, "write_u_word", 42, 42),
        "reg_s_word": RegisterTestCase(-11, "write_s_word", -42, -42),
        "reg_u_dword": RegisterTestCase(1001, "write_u_dword", 2002, 2002),
        "reg_s_dword": RegisterTestCase(-1001, "write_s_dword", -2002, -2002),
        "reg_u_dword_r": RegisterTestCase(3003, "write_u_dword_r", 4004, 4004),
        "reg_s_dword_r": RegisterTestCase(-3003, "write_s_dword_r", -4004, -4004),
        "reg_u_qword": RegisterTestCase(5005, "write_u_qword", 6006, 6006),
        "reg_s_qword": RegisterTestCase(-5005, "write_s_qword", -6006, -6006),
        "reg_u_qword_r": RegisterTestCase(7007, "write_u_qword_r", 8008, 8008),
        "reg_s_qword_r": RegisterTestCase(-7007, "write_s_qword_r", -8008, -8008),
        "reg_fp32": RegisterTestCase(
            pytest.approx(1.5, abs=0.01),
            "write_fp32",
            3.14,
            pytest.approx(3.14, abs=0.01),
        ),
        "reg_fp32_r": RegisterTestCase(
            pytest.approx(2.5, abs=0.01),
            "write_fp32_r",
            6.28,
            pytest.approx(6.28, abs=0.01),
        ),
    }

    tracker = SensorTracker(list(register_test_cases.keys()))

    # Phase 1: expect initial baseline values
    initial_futures = tracker.expect_all(
        {name: case.initial_value for name, case in register_test_cases.items()}
    )
    # Phase 2: expect post-write values (registered now so on_state can match them)
    written_futures = tracker.expect_all(
        {name: case.post_write_value for name, case in register_test_cases.items()}
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities = await tracker.setup_and_start_scenario(client)

        # Wait for initial baseline values to confirm the controller <-> server
        # connection is working before issuing writes
        await tracker.await_all(initial_futures, timeout=4.0)

        # Issue write commands for all register types
        for case in register_test_cases.values():
            entity = find_entity(entities, case.write_number_name, NumberInfo)
            assert entity is not None, (
                f"{case.write_number_name} number entity not found"
            )
            client.number_command(entity.key, case.write_value)

        # Wait for sensors to reflect the written values (round-trip write+read)
        await tracker.await_all(written_futures, timeout=4.0)
        _assert_no_modbus_errors(error_log_lines, warning_log_lines)


@pytest.mark.asyncio
@pytest.mark.xfail(
    reason="Modbus parser cannot handle server responses from other devices on the bus. Fix tracked in PR #11969.",
    strict=True,
)
async def test_uart_mock_modbus_server_controller_multiple(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test server/controller functionality with multiple servers."""

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    expected_values = {"reg_u_word": 919, "reg_u_word_2": 929}
    tracker = SensorTracker(list(expected_values.keys()))
    futures = tracker.expect_all(expected_values)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        await tracker.await_all(futures)
        _assert_no_modbus_errors(error_log_lines, warning_log_lines)
