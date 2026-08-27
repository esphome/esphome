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

from aioesphomeapi import ButtonInfo, NumberInfo, SwitchInfo
import pytest

from .state_utils import SensorTracker, find_entity, wait_for_state
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
async def test_uart_mock_modbus_server_read_write(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test modbus server FC 0x17 (read/write multiple registers).

    Injects raw 0x17 request frames and checks the round-trip through the
    server's read_lambda/write_lambda, independent of how the hub dispatches
    0x17 internally:
      * one request writes reg 0x01 then reads regs 0x01+0x02 -- reg 0x01 reads
        back the just-written value (the write happens before the read per
        Modbus 6.17), and the second register is returned by the same
        multi-register read;
      * a second request writes and reads a different register block.
    """

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    tracker = SensorTracker(
        ["rw_write_1", "rw_read_1", "rw_read_2", "rw_write_3", "rw_read_3"]
    )
    futures = tracker.expect_all(
        {
            "rw_write_1": 4660,  # 0x1234 written to reg 0x0001
            "rw_read_1": 4660,  # reg 0x0001 reads back the just-written value
            "rw_read_2": 170,  # 0x00AA read from reg 0x0002 in the same request
            "rw_write_3": 22136,  # 0x5678 written to reg 0x0003
            "rw_read_3": 22136,  # reg 0x0003 reads back the just-written value
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
async def test_uart_mock_modbus_server_read_write_invalid(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test modbus server FC 0x17 invalid-frame handling.

    Injects a well-formed (valid CRC) 0x17 request whose write byte count (2)
    does not match 2x the write quantity (2 registers need 4 bytes), so the hub
    must reject it with ILLEGAL_DATA_VALUE before touching any register. A valid
    read is injected right after as a processing marker.

    The invalid frame is verified via bus-level signals rather than the reply
    frame on the wire: the mock UART cannot observe the server's TX reliably on
    the host platform (the server's transmission is gated by a millis()-based tx
    delay), so instead we assert the request is rejected exactly once and never
    applied to a register.
    """

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    tracker = SensorTracker(["write_seen", "probe"])
    probe_seen = tracker.expect("probe", 1)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        # The probe read is injected after the malformed frame, so once it fires
        # the malformed frame has already been processed.
        await tracker.await_change(probe_seen, "probe")

    # Exactly one bus-level rejection for the malformed frame (no cascade)...
    invalid_warnings = [
        line for line in warning_log_lines if "Invalid number of registers" in line
    ]
    assert len(invalid_warnings) == 1, (
        "Expected exactly one invalid-frame rejection, got warnings:\n"
        + "\n".join(warning_log_lines)
    )
    assert len(error_log_lines) == 0, (
        "Expected no modbus errors, but got:\n" + "\n".join(error_log_lines)
    )
    # ...and the rejected write is never applied to the target register.
    assert not tracker.sensor_states["write_seen"], (
        f"malformed 0x17 must not write, but write_seen fired: {tracker.sensor_states['write_seen']}"
    )


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
        # The controller polls from boot, so the first values can already be in
        # the states the device sends on connect; matching them there saves
        # waiting for the next poll
        await tracker.setup_and_start_scenario(client, match_initial_states=True)
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
        # The controller polls from boot, so the baseline can already be in the
        # states the device sends on connect; matching it there saves waiting for
        # the next poll
        entities = await tracker.setup_and_start_scenario(
            client, match_initial_states=True
        )

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
async def test_uart_mock_modbus_server_controller_bits(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test coil/discrete-input round trips between controller and server bits.

    The server serves four bits from one shared table. The controller reads
    each of them both as a coil (FC 0x01) and as a discrete input (FC 0x02),
    so the two views must always agree. Two bits are then written back, one
    via the single-coil write (FC 0x05) and one via the multiple-coils write
    (FC 0x0F), and the new values must show up in both read views.
    """

    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    initial_values = {
        "bit_coil_0": True,
        "bit_coil_1": False,
        "bit_coil_2": False,
        "bit_coil_3": True,
        "bit_di_0": True,
        "bit_di_1": False,
        "bit_di_2": False,
        "bit_di_3": True,
    }
    tracker = SensorTracker(list(initial_values.keys()))

    # Phase 1: expect initial baseline values in both read views
    initial_futures = tracker.expect_all(initial_values)
    # Phase 2: expect post-write values (registered now so on_state can match them)
    written_futures = tracker.expect_all(
        {
            "bit_coil_2": True,
            "bit_di_2": True,
            "bit_coil_3": False,
            "bit_di_3": False,
        }
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        # The controller polls from boot and binary sensors drop repeats, so the
        # baseline can arrive only in the states the device sends on connect
        entities = await tracker.setup_and_start_scenario(
            client, match_initial_states=True
        )

        # Wait for initial baseline values to confirm the controller <-> server
        # connection is working before issuing writes
        await tracker.await_all(initial_futures, timeout=4.0)

        # Flip both writable bits: 0x02 false -> true, 0x03 true -> false
        for switch_name, value in (("write_bit_2", True), ("write_bit_3", False)):
            entity = find_entity(entities, switch_name, SwitchInfo)
            assert entity is not None, f"{switch_name} switch entity not found"
            client.switch_command(entity.key, value)

        # Wait for both read views to reflect the written values
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
        # The controller polls from boot, so the first values can already be in
        # the states the device sends on connect; matching them there saves
        # waiting for the next poll
        await tracker.setup_and_start_scenario(client, match_initial_states=True)
        await tracker.await_all(futures)
        _assert_no_modbus_errors(error_log_lines, warning_log_lines)


@pytest.mark.asyncio
async def test_uart_mock_modbus_client_typed(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test the typed modbus_client actions end to end (each action its own hub device).

    Start Scenario fires three typed actions: write_single_register puts 777 in server register 0x10 (the
    ack fires on_response -> ack_flag); read_holding_registers reads it back,
    with the reply decoded by the shared device dispatch into host-order words (values[0] -> typed_value);
    a read of unserved register 0x99 resolves via on_error with the device's exception code
    (ILLEGAL_DATA_ADDRESS = 2 -> error_code); a coil read of the register-only server resolves via
    on_error with ILLEGAL_FUNCTION (= 1 -> coil_error_code) - the server maps no bits, so it does not
    implement the coil function - proving the bit-read request and typed error delivery. A multi-register
    write (fc 0x10) lands on registers 0x11/0x12 with the read-back of 0x12 chained inside its ack handler
    (-> multi_value = 222); a multi-coil write likewise draws ILLEGAL_FUNCTION from the register-only server
    (-> multi_coil_error = 1). A read whose count lambda returns 0 at runtime
    builds an empty (rejected) PDU, is refused at the hub door, and resolves via on_not_sent
    (-> not_sent_flag).
    """

    tracker = SensorTracker(
        [
            "typed_value",
            "ack_flag",
            "error_code",
            "coil_error_code",
            "multi_value",
            "multi_coil_error",
            "not_sent_flag",
        ]
    )
    futures = tracker.expect_all(
        {
            "typed_value": 777,
            "ack_flag": 1,
            "error_code": 2,
            "coil_error_code": 1,
            "multi_value": 222,
            "multi_coil_error": 1,
            "not_sent_flag": 1,
        }
    )

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        await tracker.await_all(futures)


@pytest.mark.asyncio
async def test_uart_mock_modbus_client_inline(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test modbus_client.send actions: each action is its own hub device.

    Start Scenario fires: a read of served address 1 decoded in its inline on_response -> inline_value; a
    read of address 2, which no server answers, resolving via on_no_response -> timeout_flag. A parallel
    script fires the same write action twice while its first frame is pending; the hub drops the duplicate
    write, and the second firing resolves via its own on_not_sent -> skipped_flag. This exercises
    per-action reply routing, the no-reply path, and the one-outcome guarantee under the hub's write
    dedup.
    """

    tracker = SensorTracker(["inline_value", "timeout_flag", "skipped_flag"])
    futures = tracker.expect_all(
        {"inline_value": 1234, "timeout_flag": 1, "skipped_flag": 1}
    )

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        await tracker.await_all(futures, timeout=5.0)


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

    A word and a dword sharing 0x200 widen that range to two registers and both decode from the one read.
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
        "widen_word": 321,
        "widen_dword": pytest.approx(21037058),
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
async def test_uart_mock_modbus_custom_pdu(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test a custom_pdu sensor reading a register served by the mock server.

    The custom_pdu is a raw read-holding PDU (function code + address + count); the
    controller prepends its own device address and appends the CRC, sends it, and the
    sensor's lambda parses the response payload. Confirms the custom PDU path decodes
    the function code and routes the response to the sensor (the gap that hid the
    step-2 raw-vs-PDU bug). A plain read on the same register anchors the bus.
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
async def test_uart_mock_modbus_continuous(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that `continuous: true` polls faster than the update_interval.

    The controller's update_interval is 30s, so without continuous polling only the boot poll would
    run during the short test window. With continuous the read is re-queued after each success, filling
    idle bus time, so many reads arrive. The server returns an incrementing counter, so every read is a
    distinct published state the tracker can count. (Bus warnings are not asserted here: continuous
    polling deliberately saturates the bus, so the occasional timing hiccup is expected and off-topic;
    the other tests cover clean operation at normal poll rates.)
    """

    tracker = SensorTracker(["continuous_reg"])

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        # setup_and_start_scenario presses the Start Scenario button, whose on_press triggers the
        # controller's first update(). With continuous that one read re-queues and streams; without it
        # the next poll would not run until the 30s update_interval elapses.
        entities = await tracker.setup_and_start_scenario(client)
        # Count reads over a window far shorter than the update_interval. Absent continuous polling we
        # would see ~1 (the triggered poll); continuous re-queues, so the bus fills with reads.
        await asyncio.sleep(3.0)
        reads = len(tracker.sensor_states["continuous_reg"])
        assert reads >= 5, (
            "expected many continuous reads within the window (update_interval is 30s, so absent "
            f"continuous polling we would see ~1), got {reads}"
        )

        # Recovery path: a live continuous poll that starts failing goes offline, and the next update()
        # re-arms it once the device answers again. Silence the server so the poll's reads time out; with
        # max_cmd_retries=1 and send_wait_time=100ms the device trips offline quickly and streaming stops.
        silence = find_entity(entities, "silence_server", SwitchInfo)
        assert silence is not None, "Silence Server switch not found"
        start = find_entity(entities, "start_scenario", ButtonInfo)
        assert start is not None, "Start Scenario button not found"

        client.switch_command(silence.key, True)
        await asyncio.sleep(1.0)  # let the poll fail and the device trip offline
        plateau = len(tracker.sensor_states["continuous_reg"])
        await asyncio.sleep(1.0)  # offline: no polls should land
        assert len(tracker.sensor_states["continuous_reg"]) == plateau, (
            "reads kept arriving after the server was silenced - the failed continuous poll did not stop"
        )

        # Answer again and trigger update(): the offline probe recovers the device and the continuous
        # poll re-arms, so streaming resumes.
        client.switch_command(silence.key, False)
        client.button_command(start.key)
        await asyncio.sleep(3.0)
        resumed = len(tracker.sensor_states["continuous_reg"]) - plateau
        assert resumed >= 5, (
            f"continuous polling did not resume after the device recovered (got {resumed} new reads)"
        )


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

    Offline probing follows the offline cadence alone; regular every-update polling
    resumes once the device answers again.
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


@pytest.mark.asyncio
async def test_uart_mock_modbus_broadcast_write(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A client broadcast write (address 0) reaches every server and costs no timeout.

    The scenario button sends a broadcast single-register write of 777 to register
    0x10; both servers must apply it. The client's normal polling sensor must keep
    updating, and no modbus warnings may appear - the pre-broadcast-support behavior
    parked the frame in the waiting slot until the send-wait timeout, which surfaced
    here as 'Stop waiting for response' warnings and a stalled poll.
    """
    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    tracker = SensorTracker(
        ["reg_u_word", "srv1_written", "srv2_written", "broadcast_accepted"]
    )
    poll_before = tracker.expect("reg_u_word", 919)
    written = tracker.expect_all({"srv1_written": 777, "srv2_written": 777})
    # queue_pdu() must accept the broadcast into the machine (return true), the answer this PR adds.
    accepted = tracker.expect("broadcast_accepted", 1)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        await tracker.setup_and_start_scenario(client)
        await tracker.await_change(accepted, "broadcast_accepted")
        await tracker.await_change(poll_before, "reg_u_word")
        await tracker.await_all(written)
        # Polling must continue after the broadcast (a burned timeout stalls it).
        poll_after = tracker.expect("reg_u_word", 919)
        await tracker.await_change(poll_after, "reg_u_word", timeout=3.0)
        _assert_no_modbus_errors(error_log_lines, warning_log_lines)


@pytest.mark.asyncio
async def test_uart_mock_modbus_client_read_write(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A modbus_client.read_write_multiple_registers action (FC 0x17) drives a server end to end.

    The client writes reg 0x0001 = 0x1234 and reads regs 0x0001..0x0002 in one transaction; the server
    applies the write first (Modbus 6.17). The test confirms both ends: the server's write_lambda ran
    (srv_write_1) and the read half came back to the client's on_response (client_read_0 = the
    just-written 0x1234, client_read_1 = the read-only 0x00AA).
    """
    line_callback, error_log_lines, warning_log_lines = _make_modbus_line_callback()

    tracker = SensorTracker(
        ["srv_write_1", "srv_read_1", "client_read_0", "client_read_1"]
    )
    futures = tracker.expect_all(
        {
            "srv_write_1": 4660,  # server wrote 0x1234 to reg 0x0001
            "client_read_0": 4660,  # client read reg 0x0001 back as the just-written 0x1234
            "client_read_1": 170,  # client read reg 0x0002 (0x00AA) in the same request
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
async def test_uart_mock_modbus_register_offset(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that a byte offset on a holding-register write is byte-accurate.

    `offset` is a byte offset, so a holding-register write at address 0x10 with offset: 2 must target
    register 0x10 + 2/2 = 0x11. The pre-fix behavior folded the byte offset into the address as a register
    count (0x10 + 2 = 0x12). The switch is assumed_state (write-only), so reg_11 turning 0xFFFF pins the
    fix; had the write landed on 0x12 the wait would time out and reg_12 would change instead.
    """

    tracker = SensorTracker(["reg_10", "reg_11", "reg_12"])
    initial = tracker.expect_all({"reg_10": 100, "reg_11": 200, "reg_12": 300})
    wrote_11 = tracker.expect("reg_11", 65535)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities = await tracker.setup_and_start_scenario(client)
        await tracker.await_all(initial, timeout=4.0)

        switch = find_entity(entities, "offset_switch", SwitchInfo)
        assert switch is not None, "offset_switch not found"
        client.switch_command(switch.key, True)

        # reg_11 (0x10 + offset 2/2) must receive the write; if the write went to 0x12 this times out.
        await tracker.await_change(wrote_11, "reg_11", timeout=4.0)
        # And 0x12 (the pre-fix register-offset target) must be untouched.
        assert tracker.sensor_states["reg_12"][-1] == 300, (
            "reg_12 (0x12) should be untouched - offset is byte-based, so the write targets 0x11; "
            f"got {tracker.sensor_states['reg_12']}"
        )

        # Read path: read_offset_switch has byte offset 6. Post-fix the switch folds the whole registers
        # into its address (0x10 + 6/2 = 0x13, residual byte 0) and joins the 0x10..0x13 range, so the
        # read lands in-bounds on 0xABCD (bit 0 set) -> ON. Pre-fix the whole byte offset folded into the
        # address (0x16); the server answers ILLEGAL_DATA_ADDRESS there and the switch never publishes.
        read_switch = find_entity(entities, "read_offset_switch", SwitchInfo)
        assert read_switch is not None, "read_offset_switch not found"
        # The ON transition happened at the first poll and switch states are deduped, so this relies on
        # wait_for_state's fresh subscribe_states re-dumping every entity's current state.
        await wait_for_state(
            client,
            lambda s: (
                getattr(s, "key", None) == read_switch.key
                and getattr(s, "state", None) is True
            ),
            timeout=6.0,
        )


@pytest.mark.asyncio
async def test_uart_mock_modbus_lambda_write(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test a write_lambda that drives the write through the entity itself (item is the command).

    `cross_switch` is a coil-type switch whose write_lambda ignores its own type and calls
    item->write_single_register(0x30, ...) - a register write issued from a coil entity. The lambda
    returns an empty optional, so the write path detects the lambda already dispatched a frame and does
    not fall back to the default coil write. Success is reg_30 reading back the value the lambda wrote,
    which proves both the new item->write_* path and cross-type flexibility.
    """

    tracker = SensorTracker(["reg_30"])
    initial = tracker.expect("reg_30", 0)
    wrote_30 = tracker.expect("reg_30", 1234)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities = await tracker.setup_and_start_scenario(client)
        await tracker.await_change(initial, "reg_30", timeout=4.0)

        switch = find_entity(entities, "cross_switch", SwitchInfo)
        assert switch is not None, "cross_switch not found"
        client.switch_command(switch.key, True)

        # The coil switch's lambda wrote register 0x30 via item->write_single_register(); reg_30 must
        # read back 1234. If the entity-as-command dispatch were broken, no register write would go out
        # and this would time out.
        await tracker.await_change(wrote_30, "reg_30", timeout=4.0)


@pytest.mark.asyncio
async def test_uart_mock_modbus_lambda_invert(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that a write_lambda's return value is the wire value only.

    `invert_switch` is an active-low holding switch whose write_lambda returns !x. Turning it ON must
    write 0x0000 to the register (observed through the independent reg_40 sensor) while the entity
    reports ON - the requested state, not the inverted wire value. Turning it OFF writes 0xFFFF and
    reports OFF. The switch is assumed_state, so the published state comes only from write_state().
    """

    tracker = SensorTracker(["reg_40"])
    initial = tracker.expect("reg_40", 5)
    wrote_on = tracker.expect("reg_40", 0)
    wrote_off = tracker.expect("reg_40", 65535)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities = await tracker.setup_and_start_scenario(client)
        await tracker.await_change(initial, "reg_40", timeout=4.0)

        switch = find_entity(entities, "invert_switch", SwitchInfo)
        assert switch is not None, "invert_switch not found"

        client.switch_command(switch.key, True)
        # The wire byte carries the inverted value...
        await tracker.await_change(wrote_on, "reg_40", timeout=4.0)
        # ...while the entity reports the requested state. Switch states are deduped, so this relies on
        # wait_for_state's fresh subscribe_states re-dumping every entity's current state.
        await wait_for_state(
            client,
            lambda s: (
                getattr(s, "key", None) == switch.key
                and getattr(s, "state", None) is True
            ),
            timeout=6.0,
        )

        client.switch_command(switch.key, False)
        await tracker.await_change(wrote_off, "reg_40", timeout=4.0)
        await wait_for_state(
            client,
            lambda s: (
                getattr(s, "key", None) == switch.key
                and getattr(s, "state", None) is False
            ),
            timeout=6.0,
        )


@pytest.mark.asyncio
async def test_uart_mock_modbus_deprecated_write_buffer(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test the deprecated write_lambda buffer path still works, and warns once per entity.

    buf_number's write_lambda fills the old `payload` buffer with a legacy raw frame as words (device
    address + function code + data) and returns {} instead of calling item->write_*. Both writes must
    land - a filled buffer is sent, as the docs have always described - and the one-time deprecation
    warning must fire exactly once per entity regardless of how many writes happen.
    """

    warn_count = 0

    def line_callback(line: str) -> None:
        nonlocal warn_count
        if "write_lambda buffer" in line:
            warn_count += 1

    tracker = SensorTracker(["written_value"])

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities = await tracker.setup_and_start_scenario(client)
        number = find_entity(entities, "buf_number", NumberInfo)
        assert number is not None, "buf_number not found"

        # First write via the deprecated buffer path.
        client.number_command(number.key, 111)
        await tracker.await_change(
            tracker.expect("written_value", 111), "written_value", timeout=4.0
        )
        # Second write: lands too, but must not warn again (warn-once per entity).
        client.number_command(number.key, 222)
        await tracker.await_change(
            tracker.expect("written_value", 222), "written_value", timeout=4.0
        )

    assert warn_count == 1, (
        f"deprecation warning should fire exactly once per entity, got {warn_count}"
    )
