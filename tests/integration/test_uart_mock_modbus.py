"""Integration test for modbus component with virtual UART.

Tests:
test_uart_mock_modbus :
  1. Read a single register and parse successfully (basic_register)
  2. Read multiple registers from SDM meter and parse successfully (sdm_voltage), with some intermediate delay to simulate UART buffer time.

test_uart_mock_modbus_no_threshold :
  Test modbus with no rx_full_threshold set (simulating USB UART / non-hardware UART).
  Verifies the 50ms fallback timeout handles chunked data with USB packet gaps.

test_uart_mock_modbus_fairness :
  Two controllers sharing one client bus, both polling far faster than the bus
  can service. Verifies the hub schedules them fairly (request counts within 1).

"""

from __future__ import annotations

import asyncio
from collections.abc import Callable
from dataclasses import dataclass

from aioesphomeapi import ButtonInfo, NumberInfo
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

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    tracker = SensorTracker(["sdm_voltage"])
    voltage_changed = tracker.expect_any("sdm_voltage")

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        await tracker.await_change(voltage_changed, "sdm_voltage")
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
    Bus-level warnings (CRC/parse failures, buffer clears) are NOT expected during
    chunked reassembly, if timeouts are set properly — these warnings indicate undersized timeouts.
    """

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    tracker = SensorTracker(["sdm_voltage"])
    voltage_changed = tracker.expect_any("sdm_voltage")

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        await tracker.await_change(voltage_changed, "sdm_voltage")
        _assert_no_modbus_errors(error_log_lines, warning_log_lines)


@pytest.mark.asyncio
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
        "reg_u_word_s": 4660,
        "reg_u_word_s_raw": 13330,
        "reg_s_word": -99,
        "reg_s_word_s": -2,
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
    All 14 value types are tested: U/S_WORD, U/S_WORD_S, U/S_DWORD(_R), U/S_QWORD(_R), FP32(_R).
    """

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    register_test_cases: dict[str, RegisterTestCase] = {
        "reg_u_word": RegisterTestCase(11, "write_u_word", 42, 42),
        "reg_u_word_s": RegisterTestCase(4660, "write_u_word_s", 17185, 17185),
        "reg_s_word": RegisterTestCase(-11, "write_s_word", -42, -42),
        "reg_s_word_s": RegisterTestCase(-2, "write_s_word_s", -257, -257),
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


@pytest.mark.asyncio
async def test_uart_mock_modbus_grouping(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Pins how sensors are grouped into polled ranges across the combinations that matter.

    Each block in the fixture covers one relationship between neighbouring sensors - sharing a wide
    register, contiguous, separated by a gap, differing polling rates, coils, and a pinned range - so
    that the frames on the wire and the byte each sensor decodes from are locked down.
    """

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    # Values are those the component produced before the range rework, captured from it directly.
    expected_values = {
        # one register returning 4 bytes, read as two halves
        "reuse_lo": 273,
        "reuse_hi": 546,
        # contiguous registers, mixed widths
        "ext_word": 4660,
        "ext_next": 22136,
        "ext_dword": pytest.approx(2596069120),
        # a wide register pushes its neighbour past the bytes it actually returned
        "wide_first": 2730,
        "wide_next": 3003,
        # a gap keeps them apart
        "gap_low": 320,
        "gap_high": 325,
        # contiguous, second one polling more slowly
        "rate_first": 336,
        "rate_slow": 337,
        # a wide value, one of its halves, and the register after it
        "shared_dword": pytest.approx(2759468),
        "shared_high": 6956,
        "shared_after": 781,
        # a wide register hidden behind a wider plain sibling, and the sensor after them
        "masked_wide": 4369,
        "masked_pair": pytest.approx(286335522),
        "masked_after": 13107,
        # pinned range, and the contiguous sensor after it
        "forced_first": 352,
        "forced_next": 353,
    }
    tracker = SensorTracker(list(expected_values.keys()))
    futures = tracker.expect_all(expected_values)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        await tracker.await_all(futures)
        # Every frame sent must match one the mock answers, so an unexpected read (a range that split,
        # merged or changed length) shows up here as an unanswered request. This is what pins the coil
        # grouping too, since binary sensors carry no numeric state to compare.
        _assert_no_modbus_errors(error_log_lines, warning_log_lines)


@pytest.mark.asyncio
async def test_uart_mock_modbus_shared_address(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Sensors sharing and overlapping one register range must all decode from a single read.

    A U_WORD and a U_DWORD share start address 0x9001 (non-mergeable, so the range widens to 2
    registers) and a third U_WORD at 0x9002 falls inside the widened range. A regression guard for the
    range-grouping rewrite: without the same-address fallback the shared sensors land in duplicate
    ranges and one never publishes; without the in-range join the 0x9002 sensor splits into a second
    overlapping frame that the mock (which expects exactly one read) never answers.

    A force_new_range sensor at 0x30 plus a plain sensor at 0x10 pin the covered branch's lower-bound
    check: the forced sensor sorts first, and without the bound the lower-address sensor is absorbed
    into the forced range with a wrapped byte offset and never polls its own register.

    A U_QWORD at 0x100 with plain sensors at 0x101 and 0x103 pins that non-merging sensors inside a
    wide sensor's span keep polling separately, and that the sensor at the span's tail address does not
    anchor a re-use join on a mid-range predecessor (which would make it decode that sensor's bytes).

    A sensor at 0x201 carrying skip_updates sits inside a widened shared-address range at 0x200 but
    keeps its own range, so polling rates stay independent; folding it in would also make it decode
    0x201 out of the shared response (2) instead of its own poll (777).
    """

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    # 0x9001 = 0x0397 (919); 0x9001..0x9002 = 0x03970291 (60228241, approx: not exact in float32);
    # 0x9002 = 0x0291 (657); 0x30 = 0x0111 (273); 0x10 = 0x0222 (546)
    expected_values = {
        "shared_word": 919,
        "shared_dword": pytest.approx(60228241),
        "covered_word": 657,
        "forced_high": 273,
        "plain_low": 546,
        "wide_qword": 100,
        "inside_wide": 321,
        "tail_of_wide": 421,
        "rate_word": 321,
        "rate_dword": pytest.approx(21037058),
        "own_rate": 777,
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
async def test_uart_mock_modbus_custom_command(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test a custom_command sensor polling a register served by the mock server.

    The custom_command is a raw frame (device address + PDU); the hub appends the CRC and
    routes the response back to the polling command, whose sensor lambda parses the payload.
    Guards the custom polling wiring: the command must reference the sensor's custom_data and
    decode the real function code, or nothing is ever transmitted. A plain read on the same
    register anchors the bus.
    """

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    expected_values = {"plain_read": 259, "custom_read": 259}
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
async def test_uart_mock_modbus_offline(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A silent device drives the controller offline; answering again recovers it.

    The mock answers nothing at first, so the controller burns through max_cmd_retries
    (1 retry after the first timeout) and fires on_offline. While offline it keeps
    retrying every offline_skip_updates+1 cycles. The test then flips the mock to
    answering; the next retry gets a response, on_online fires, and the register value
    publishes. This pins the pooled non-response counter, can_send() gating, the
    offline retry cadence, and recovery - none of which the responding-path tests touch.

    The fixture gives offline_skip_updates and the sensor's skip_updates the same period
    on purpose: offline probing must follow the offline cadence alone, since requiring
    both cadences to coincide leaves phase combinations where no probe ever goes out.
    """

    tracker = SensorTracker(["link_state", "reg"])
    offline_future = tracker.expect("link_state", 0)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities = await tracker.setup_and_start_scenario(client)

        # The unanswered poll and its retry each time out (~100ms), then on_offline fires.
        await tracker.await_change(offline_future, "link_state", timeout=5.0)

        # Register the recovery expectations before waking the device so no update is missed.
        online_future = tracker.expect("link_state", 1)
        value_future = tracker.expect("reg", 259)
        serve_btn = find_entity(entities, "serve", ButtonInfo)
        assert serve_btn is not None, "Serve button not found"
        client.button_command(serve_btn.key)

        # The next offline-cadence retry gets an answer: back online, value published.
        await tracker.await_change(online_future, "link_state", timeout=5.0)
        await tracker.await_change(value_future, "reg", timeout=5.0)


@pytest.mark.asyncio
async def test_uart_mock_modbus_fairness(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Two controllers sharing one bus should get a fair share of it.

    Both controllers poll different devices (addresses 1 and 2) on the same
    client hub, far faster than the bus can service, so they continually
    contend for it. The on_tx hook in the fixture counts the requests issued
    for each address. With fair scheduling in the modbus hub, neither
    controller should starve the other: the two request counts must end up
    within 1 of each other.
    """

    tracker = SensorTracker(["requests_1", "requests_2"])

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities = await tracker.setup_and_start_scenario(client)

        # Let both controllers hammer the bus for a while.
        await asyncio.sleep(2.0)

        # Stop polling so the counters settle to a final, stable value (state
        # coalescing means intermediate values may be skipped, but the final
        # value is always delivered once changes stop).
        stop_btn = find_entity(entities, "stop_scenario", ButtonInfo)
        assert stop_btn is not None, "Stop Scenario button not found"
        client.button_command(stop_btn.key)
        await asyncio.sleep(0.5)

        assert tracker.sensor_states["requests_1"], "controller 1 issued no requests"
        assert tracker.sensor_states["requests_2"], "controller 2 issued no requests"
        count_1 = tracker.sensor_states["requests_1"][-1]
        count_2 = tracker.sensor_states["requests_2"][-1]

        # Both must have polled repeatedly, otherwise "fairness" is meaningless.
        assert count_1 >= 5 and count_2 >= 5, (
            f"expected both controllers to poll repeatedly, "
            f"got controller 1={count_1}, controller 2={count_2}"
        )
        # Fair scheduling: the bus alternates between the two pending requests,
        # so the counts can differ by at most one in-flight request.
        assert abs(count_1 - count_2) <= 1, (
            f"controllers did not get a fair share of the bus: "
            f"controller 1 issued {count_1}, controller 2 issued {count_2}"
        )
