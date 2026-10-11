"""Integration test for LD2412 component with mock UART.

Tests:
test_uart_mock_ld2412 (normal mode):
  1. Happy path - valid data frame publishes correct sensor values
  2. Garbage resilience - random bytes don't crash the component
  3. Truncated frame handling - partial frame doesn't corrupt state
  4. Buffer overflow recovery - overflow resets the parser
  5. Post-overflow parsing - next valid frame after overflow is parsed correctly
  6. TX logging - verifies LD2412 sends expected setup commands

test_uart_mock_ld2412_engineering (engineering mode):
  1. Engineering mode frames with per-gate energy data and light sensor
  2. Multi-byte still distance (291cm) using high byte > 0
  3. Gate energy sensor values
  4. Detection distance computed from target state

test_uart_mock_ld2412_engineering_truncated (truncated engineering mode):
  1. Valid engineering frame establishes baseline sensor values
  2. Truncated engineering frame (24 bytes) is rejected — gate/light sensors
     must not receive garbage from stale buffer data or frame footer bytes
  3. Recovery frame with different values proves the component survived

test_uart_mock_ld2412_gate_thresholds (partial gate threshold config):
  1. Threshold queries are answered, so every gate has a known module value
  2. Writing one threshold does not crash when most gates have no number
  3. Gates without a number are written back with the module value, not a zero

test_uart_mock_ld2412_footer_in_payload (frame footer bytes inside the thresholds):
  1. Threshold answers holding 04 03 02 01 before the real footer are parsed in full
  2. The answer that follows such a frame is still parsed
  3. Writing a threshold sends back all 14 gates exactly as read

test_uart_mock_ld2412_thresholds_not_read (module never reports its thresholds):
  1. The threshold queries go unanswered, so no gate has a module value
  2. Writing a threshold holds the command back instead of writing zeros
  3. The group where no gate has a value at all is not sent either
  4. Only the held back group is queried again, before configuration mode ends

test_uart_mock_ld2412_requery_answered (module answers only the second query):
  1. The setup query goes unanswered, so writing a threshold is held back
  2. The module answers the query sent again for the held back write
  3. The held back write is then sent with the user value and the module values
  4. The module values do not overwrite the value the user wrote

test_uart_mock_ld2412_truncated_answer (truncated threshold answer):
  1. A motion answer missing two gate bytes runs into the next acknowledgement
  2. The merged bytes are dropped with a warning, no number takes a value from them
  3. A complete motion answer that arrives later is parsed
"""

from __future__ import annotations

import asyncio
from collections.abc import Callable

from aioesphomeapi import (
    APIClient,
    ButtonInfo,
    EntityInfo,
    EntityState,
    NumberInfo,
    NumberState,
)
import pytest

from .state_utils import (
    InitialStateHelper,
    SensorStateCollector,
    StateWaiter,
    find_entity,
    require_entity,
)
from .types import APIClientConnectedFactory, RunCompiledFunction

# Frames as the uart mock logs what the component writes. The component writes the length and command word
# as one piece and the payload as the next, so each shows up on its own TX line
# Length 16 (2 command bytes and one byte per gate), then the threshold command, motion is 0x03 and still 0x04
MOTION_COMMAND = "10:00:03:00"
STILL_COMMAND = "10:00:04:00"
# Length 2 and the query command for each group, motion is 0x13 and still is 0x14
MOTION_QUERY = "02:00:13:00"
STILL_QUERY = "02:00:14:00"
# Length 2 and command 0xFE, the frame that leaves configuration mode
CONFIG_MODE_LEFT = "02:00:FE:00"
# Motion payload after writing 77 (0x4D) to gate 0, every other gate keeps the module value (11 to 23)
EXPECTED_MOTION = "4D:0B:0C:0D:0E:0F:10:11:12:13:14:15:16:17"

THRESHOLD_NUMBERS = (
    "gate_0_move_threshold",
    "gate_0_still_threshold",
    "gate_5_move_threshold",
    "gate_5_still_threshold",
)


def _is_tx(line: str) -> bool:
    """Return whether the log line is a TX line from the uart mock."""
    return "uart_mock" in line and "TX " in line


def _tx_waiter(
    *payloads: str,
) -> tuple[Callable[[str], None], list[asyncio.Future[bool]]]:
    """Return a line callback and one future per payload, resolved when a TX line holds it."""
    loop = asyncio.get_running_loop()
    futures = [loop.create_future() for _ in payloads]

    def line_callback(line: str) -> None:
        if not _is_tx(line):
            return
        for payload, future in zip(payloads, futures, strict=True):
            if payload in line and not future.done():
                future.set_result(True)

    return line_callback, futures


def _threshold_numbers(entities: list[EntityInfo]) -> dict[str, NumberInfo]:
    """Return the gate 0 and gate 5 threshold numbers by name."""
    return {
        name: require_entity(entities, name, NumberInfo) for name in THRESHOLD_NUMBERS
    }


async def _wait_initial(
    client: APIClient,
    entities: list[EntityInfo],
    on_state: Callable[[EntityState], None] = lambda s: None,
) -> InitialStateHelper:
    """Subscribe to states and wait until every entity has sent its initial state."""
    initial_state_helper = InitialStateHelper(entities)
    client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
    try:
        await initial_state_helper.wait_for_initial_states()
    except TimeoutError:
        pytest.fail("Timeout waiting for initial states")
    return initial_state_helper


def _number_state_of(key: int) -> Callable[[EntityState], bool]:
    """Return a predicate that matches a number state with a value for ``key``."""
    return lambda s: isinstance(s, NumberState) and not s.missing_state and s.key == key


@pytest.mark.asyncio
async def test_uart_mock_ld2412(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2412 data parsing with happy path, garbage, overflow, and recovery."""
    loop = asyncio.get_running_loop()

    # Track overflow warning in logs
    overflow_seen = loop.create_future()

    # Track TX data logged by the mock for assertions
    tx_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "Max command length exceeded" in line and not overflow_seen.done():
            overflow_seen.set_result(True)
        # Capture all TX log lines from uart_mock
        if _is_tx(line):
            tx_log_lines.append(line)

    collector = SensorStateCollector(
        sensor_names=[
            "moving_distance",
            "still_distance",
            "moving_energy",
            "still_energy",
            "detection_distance",
        ],
        binary_sensor_names=[
            "has_target",
            "has_moving_target",
            "has_still_target",
        ],
    )

    # Signal when we see all recovery frame values
    recovery_received = collector.add_waiter(
        lambda: (
            pytest.approx(50.0) in collector.sensor_states["moving_distance"]
            and pytest.approx(75.0) in collector.sensor_states["still_distance"]
            and pytest.approx(100.0) in collector.sensor_states["moving_energy"]
            and pytest.approx(80.0) in collector.sensor_states["still_energy"]
            and pytest.approx(50.0) in collector.sensor_states["detection_distance"]
        )
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)

        await _wait_initial(client, entities, collector.on_state)

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        # Wait for Phase 1 - all sensors and binary sensors have at least one value
        try:
            await collector.wait_for_all(timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 1 frame. Received:\n"
                f"  sensor_states: {collector.sensor_states}\n"
                f"  binary_states: {collector.binary_states}"
            )

        # Phase 1 values: moving=100, still=120, energy=50/25, detect=100
        assert collector.sensor_states["moving_distance"][0] == pytest.approx(100.0)
        assert collector.sensor_states["still_distance"][0] == pytest.approx(120.0)
        assert collector.sensor_states["moving_energy"][0] == pytest.approx(50.0)
        assert collector.sensor_states["still_energy"][0] == pytest.approx(25.0)
        assert collector.sensor_states["detection_distance"][0] == pytest.approx(100.0)

        # Wait for the recovery frame (Phase 5) to be parsed
        # This proves the component survived garbage + truncated + overflow
        try:
            await asyncio.wait_for(recovery_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for recovery frame. Received:\n"
                f"  sensor_states: {collector.sensor_states}"
            )

        # Verify overflow warning was logged
        assert overflow_seen.done(), (
            "Expected 'Max command length exceeded' warning in logs"
        )

        # Verify LD2412 sent setup commands (TX logging)
        assert len(tx_log_lines) > 0, "Expected TX log lines from uart_mock"
        tx_data = " ".join(tx_log_lines)
        assert "FD:FC:FB:FA" in tx_data, (
            "Expected LD2412 command frame header FD:FC:FB:FA in TX log"
        )
        assert "04:03:02:01" in tx_data, (
            "Expected LD2412 command frame footer 04:03:02:01 in TX log"
        )

        # Recovery frame: moving=50, still=75, energy=100/80, detect=50
        # Check values exist (waiter already ensured all are present)
        assert pytest.approx(50.0) in collector.sensor_states["moving_distance"]
        assert pytest.approx(75.0) in collector.sensor_states["still_distance"]
        assert pytest.approx(100.0) in collector.sensor_states["moving_energy"]
        assert pytest.approx(80.0) in collector.sensor_states["still_energy"]
        assert pytest.approx(50.0) in collector.sensor_states["detection_distance"]

        # Verify binary sensors detected targets (from Phase 1 frame)
        assert collector.binary_states["has_target"][0] is True
        assert collector.binary_states["has_moving_target"][0] is True
        assert collector.binary_states["has_still_target"][0] is True


@pytest.mark.asyncio
async def test_uart_mock_ld2412_engineering(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2412 engineering mode with per-gate energy, light, and multi-byte distance."""

    collector = SensorStateCollector(
        sensor_names=[
            "moving_distance",
            "still_distance",
            "moving_energy",
            "still_energy",
            "detection_distance",
            "light",
            "gate_0_move_energy",
            "gate_1_move_energy",
            "gate_2_move_energy",
            "gate_0_still_energy",
            "gate_1_still_energy",
            "gate_2_still_energy",
        ],
        binary_sensor_names=[
            "has_target",
            "has_moving_target",
            "has_still_target",
        ],
    )

    # Signal when we see Phase 3 frame values
    phase3_still_received = collector.add_waiter(
        lambda: pytest.approx(291.0) in collector.sensor_states["still_distance"]
    )
    phase3_detect_received = collector.add_waiter(
        lambda: pytest.approx(291.0) in collector.sensor_states["detection_distance"]
    )

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)

        await _wait_initial(client, entities, collector.on_state)

        # Start the UART mock scenario now that we're subscribed
        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        # Wait for Phase 1 - all sensors and binary sensors have at least one value
        try:
            await collector.wait_for_all(timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 1 frame. Received:\n"
                f"  sensor_states: {collector.sensor_states}\n"
                f"  binary_states: {collector.binary_states}"
            )

        # Phase 1 values (engineering mode frame):
        # moving=30, energy=100, still=30, energy=100, detect=30
        assert collector.sensor_states["moving_distance"][0] == pytest.approx(30.0)
        assert collector.sensor_states["still_distance"][0] == pytest.approx(30.0)
        assert collector.sensor_states["gate_0_move_energy"][0] == pytest.approx(100.0)
        assert collector.sensor_states["gate_1_move_energy"][0] == pytest.approx(65.0)
        assert collector.sensor_states["light"][0] == pytest.approx(87.0)

        # Wait for Phase 3 frame: still_distance = 291cm (multi-byte)
        try:
            await asyncio.wait_for(phase3_still_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 3 still_distance. Received:\n"
                f"  still_distance: {collector.sensor_states['still_distance']}"
            )

        assert pytest.approx(291.0) in collector.sensor_states["still_distance"]

        # Wait for Phase 3: detection_distance = 291 (still-only target)
        try:
            await asyncio.wait_for(phase3_detect_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for detection_distance=291. "
                f"Received: {collector.sensor_states['detection_distance']}"
            )

        assert pytest.approx(291.0) in collector.sensor_states["detection_distance"]


@pytest.mark.asyncio
async def test_uart_mock_ld2412_engineering_truncated(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that truncated engineering mode frames don't corrupt sensor values.

    Without the fix, a 24-byte engineering mode frame passes the old buffer_pos_ >= 12
    check but reads indices 17-45 from stale buffer data, publishing garbage values
    (e.g. frame footer bytes 0xF8=248 as gate energy).
    """

    loop = asyncio.get_running_loop()

    # Track the truncated frame warning
    truncated_warning_seen = loop.create_future()

    def line_callback(line: str) -> None:
        if (
            "Engineering mode packet too short" in line
            and not truncated_warning_seen.done()
        ):
            truncated_warning_seen.set_result(True)

    collector = SensorStateCollector(
        sensor_names=[
            "moving_distance",
            "still_distance",
            "moving_energy",
            "still_energy",
            "detection_distance",
            "light",
            "gate_0_move_energy",
            "gate_0_still_energy",
        ],
        binary_sensor_names=[
            "has_target",
            "has_moving_target",
            "has_still_target",
        ],
    )

    # Signal when we see ALL Phase 3 recovery values to avoid race where some
    # arrive after the waiter fires but before we index into the lists
    recovery_received = collector.add_waiter(
        lambda: (
            pytest.approx(50.0) in collector.sensor_states["gate_0_move_energy"]
            and pytest.approx(42.0) in collector.sensor_states["light"]
        )
    )

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)

        await _wait_initial(client, entities, collector.on_state)

        start_btn = find_entity(entities, "start_scenario", ButtonInfo)
        assert start_btn is not None, "Start Scenario button not found"
        client.button_command(start_btn.key)

        # Wait for Phase 1 — valid engineering frame establishes baseline
        try:
            await collector.wait_for_all(timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for Phase 1 frame. Received:\n"
                f"  sensor_states: {collector.sensor_states}\n"
                f"  binary_states: {collector.binary_states}"
            )

        # Phase 1 baseline: gate_0_move=100, light=87
        assert collector.sensor_states["gate_0_move_energy"][0] == pytest.approx(100.0)
        assert collector.sensor_states["light"][0] == pytest.approx(87.0)

        # Wait for Phase 3 recovery frame (gate_0_move=50)
        try:
            await asyncio.wait_for(recovery_received, timeout=3.0)
        except TimeoutError:
            pytest.fail(
                f"Timeout waiting for recovery frame. Received:\n"
                f"  gate_0_move_energy: {collector.sensor_states['gate_0_move_energy']}\n"
                f"  light: {collector.sensor_states['light']}"
            )

        # Verify the truncated frame warning was logged
        assert truncated_warning_seen.done(), (
            "Expected 'Engineering mode packet too short' warning in logs"
        )

        # Phase 3 recovery: gate_0_move=50, light=42
        assert pytest.approx(50.0) in collector.sensor_states["gate_0_move_energy"]
        assert pytest.approx(42.0) in collector.sensor_states["light"]

        # The critical assertion: gate_0_move_energy must never have received
        # garbage values from the truncated frame. Without the fix,
        # buffer_data_[17] = 0xFF = 255 would be published as gate_0_move.
        for value in collector.sensor_states["gate_0_move_energy"]:
            assert value == pytest.approx(100.0) or value == pytest.approx(50.0), (
                f"gate_0_move_energy got unexpected value {value} — "
                f"truncated frame likely leaked stale buffer data. "
                f"All values: {collector.sensor_states['gate_0_move_energy']}"
            )


@pytest.mark.asyncio
async def test_uart_mock_ld2412_gate_thresholds(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test writing a threshold when only 2 of the 14 gates have numbers configured.

    The threshold commands carry all 14 gates, so the component has to produce a value for every gate.
    It used to dereference the numbers of gates that were never configured, which crashed on the first
    write. The gates without a number must be written back with the value read from the module.
    """
    # Still: nothing was changed, so every gate keeps the module value (40 to 53)
    expected_still = "28:29:2A:2B:2C:2D:2E:2F:30:31:32:33:34:35"
    line_callback, written = _tx_waiter(EXPECTED_MOTION, expected_still)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        initial_state_helper = await _wait_initial(client, entities)
        numbers = _threshold_numbers(entities)

        # The setup queries were answered, so the 2 configured gates show the module values
        for name, value in {
            "gate_0_move_threshold": 10.0,
            "gate_5_move_threshold": 15.0,
            "gate_0_still_threshold": 40.0,
            "gate_5_still_threshold": 45.0,
        }.items():
            state = initial_state_helper.initial_states[numbers[name].key]
            assert state.state == pytest.approx(value), name

        # Write one threshold; this is what used to crash the device
        client.number_command(numbers["gate_0_move_threshold"].key, 77.0)

        try:
            await asyncio.wait_for(asyncio.gather(*written), timeout=5.0)
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for the threshold commands.\n"
                f"  expected motion payload: {EXPECTED_MOTION}\n"
                f"  expected still payload:  {expected_still}"
            )

        # A round trip proves the device is still running after the write
        entities_after, _ = await client.list_entities_services()
        assert len(entities_after) == len(entities)


@pytest.mark.asyncio
async def test_uart_mock_ld2412_footer_in_payload(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test threshold answers whose values contain the command frame footer.

    Gates holding 4, 3, 2, 1 in a row look like the footer 04 03 02 01, so the parser sees a footer
    before the frame is complete. A short frame is not a complete answer, so it has to keep reading up
    to the real footer, parse all 14 thresholds and then parse the next answer as well.
    """
    # Every gate except the changed gate 0 must match the answer byte for byte, which proves all 14
    # thresholds were parsed
    # Motion: gate 0 becomes 77 (0x4D), gates 5 to 8 keep 4, 3, 2, 1
    expected_motion = "4D:23:23:23:23:04:03:02:01:19:19:19:19:19"
    # Still: nothing was changed, gates 10 to 13 keep 4, 3, 2, 1
    expected_still = "00:23:23:23:23:19:19:19:19:19:04:03:02:01"
    line_callback, written = _tx_waiter(expected_motion, expected_still)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        initial_state_helper = await _wait_initial(client, entities)
        numbers = _threshold_numbers(entities)

        # Gate 5 move holds the first footer byte, gate 5 still comes from the second answer
        for name, value in {
            "gate_0_move_threshold": 0.0,
            "gate_5_move_threshold": 4.0,
            "gate_0_still_threshold": 0.0,
            "gate_5_still_threshold": 25.0,
        }.items():
            state = initial_state_helper.initial_states[numbers[name].key]
            assert state.state == pytest.approx(value), name

        client.number_command(numbers["gate_0_move_threshold"].key, 77.0)

        try:
            await asyncio.wait_for(asyncio.gather(*written), timeout=5.0)
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for the threshold commands.\n"
                f"  expected motion payload: {expected_motion}\n"
                f"  expected still payload:  {expected_still}"
            )


@pytest.mark.asyncio
async def test_uart_mock_ld2412_thresholds_not_read(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test writing a threshold before the module has reported the values it holds.

    The gates without a number are filled from the last values read back from the module. When the
    module never answers the setup query there is nothing to fill them with, and sending the command
    anyway would write a zero to every one of those gates, which is maximum sensitivity. The command
    has to be held back instead, and only that group is queried again. The still group has no gate with
    a value at all, so it is neither sent nor queried.
    """
    loop = asyncio.get_running_loop()

    motion_held_back = loop.create_future()
    write_finished = loop.create_future()
    tx_lines: list[str] = []
    # TX lines sent between the warning and leaving configuration mode
    write_tx_lines: list[str] = []

    def line_callback(line: str) -> None:
        # The component logs the command it did not send in place of sending it
        if "Command 03 held back" in line and not motion_held_back.done():
            motion_held_back.set_result(True)
            return
        if not _is_tx(line):
            return
        tx_lines.append(line)
        if not motion_held_back.done() or write_finished.done():
            return
        # Configuration mode is left once both groups have been dealt with
        if CONFIG_MODE_LEFT in line:
            write_finished.set_result(True)
            return
        write_tx_lines.append(line)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        gate_0_move = require_entity(entities, "gate_0_move_threshold", NumberInfo)

        client.number_command(gate_0_move.key, 77.0)

        try:
            await asyncio.wait_for(
                asyncio.gather(motion_held_back, write_finished), timeout=5.0
            )
        except TimeoutError:
            pytest.fail("Timeout waiting for the write to be handled")

        # Neither threshold command reached the module, so no gate was reconfigured
        assert not any(MOTION_COMMAND in line for line in tx_lines), (
            "motion threshold command was sent without a value for the gates that have no number"
        )
        assert not any(STILL_COMMAND in line for line in tx_lines), (
            "still threshold command was sent when no gate had a value to write"
        )

        # Only the held back motion group asks the module for its values again
        assert any(MOTION_QUERY in line for line in write_tx_lines), (
            "motion thresholds were not queried again after the command was held back"
        )
        assert not any(STILL_QUERY in line for line in write_tx_lines), (
            "still thresholds were queried although that group was not held back"
        )

        # A round trip proves the device is still running after the write
        entities_after, _ = await client.list_entities_services()
        assert len(entities_after) == len(entities)


@pytest.mark.asyncio
async def test_uart_mock_ld2412_requery_answered(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that a held back threshold write is sent once the module answers the query again.

    The module ignores the motion query sent during setup, so the first write is held back and the
    motion thresholds are queried again. The module answers that query, and the held back write has to
    go out with the value the user wrote for gate 0 and the module values for every other gate. The
    answer must not overwrite the value the user wrote.
    """
    loop = asyncio.get_running_loop()

    held_back = "Command 03 held back"
    # Lines in the order they were logged: the warning and every TX line
    events: list[str] = []
    motion_written = loop.create_future()

    def line_callback(line: str) -> None:
        if held_back in line:
            events.append(line)
            return
        if not _is_tx(line):
            return
        events.append(line)
        if (
            EXPECTED_MOTION in line
            and len(events) >= 2
            and MOTION_COMMAND in events[-2]
            and not motion_written.done()
        ):
            motion_written.set_result(True)

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()

        gate_0_move = require_entity(entities, "gate_0_move_threshold", NumberInfo)
        gate_5_move = require_entity(entities, "gate_5_move_threshold", NumberInfo)

        gate_0_values: list[float] = []
        waiter = StateWaiter()

        def on_state(state: EntityState) -> None:
            if _number_state_of(gate_0_move.key)(state):
                gate_0_values.append(state.state)
            waiter.on_state(state)

        initial_state_helper = await _wait_initial(client, entities, on_state)

        # Neither query was answered during setup, so no threshold has a value yet
        for key in (gate_0_move.key, gate_5_move.key):
            assert initial_state_helper.initial_states[key].missing_state

        gate_5_read = waiter.expect(
            _number_state_of(gate_5_move.key), label="gate 5 move threshold"
        )
        client.number_command(gate_0_move.key, 77.0)

        try:
            await asyncio.wait_for(motion_written, timeout=5.0)
            gate_5_state = await gate_5_read
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for the held back motion write.\n"
                f"  expected payload: {MOTION_COMMAND} then {EXPECTED_MOTION}\n"
                f"  logged: {events}"
            )

        # The warning, the query sent again and the write arrive in that order
        warning_at = next(i for i, line in enumerate(events) if held_back in line)
        query_at = next(
            i
            for i, line in enumerate(events)
            if i > warning_at and MOTION_QUERY in line
        )
        write_at = next(i for i, line in enumerate(events) if EXPECTED_MOTION in line)
        assert warning_at < query_at < write_at

        assert not any(STILL_COMMAND in line for line in events), (
            "still threshold command was sent although no still gate has a value"
        )

        # The user value survives the answer, gate 5 takes the module value
        assert gate_0_values
        assert all(value == pytest.approx(77.0) for value in gate_0_values)
        assert gate_5_state.state == pytest.approx(15.0)


@pytest.mark.asyncio
async def test_uart_mock_ld2412_truncated_answer(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test recovery from a truncated threshold answer that runs into the next answer.

    The motion answer is missing two gate bytes, so its footer comes too early and the next acknowledgement
    is read as part of the same frame. Those bytes must be dropped with a warning and must not reach any
    number. A complete motion answer that arrives later has to be parsed, which proves the parser
    recovered.
    """
    loop = asyncio.get_running_loop()

    dropped = loop.create_future()
    warnings: list[str] = []

    def line_callback(line: str) -> None:
        if "[W][ld2412" in line:
            warnings.append(line)
        if "Dropping gate threshold answer" in line and not dropped.done():
            dropped.set_result(True)

    # Values the merged frame carries in its motion part
    merged_values = set(range(0x30, 0x3C))

    async with (
        run_compiled(yaml_config, line_callback=line_callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        numbers = _threshold_numbers(entities)

        states: dict[int, list[float]] = {}
        waiter = StateWaiter()
        gate_0_read = waiter.expect(
            _number_state_of(numbers["gate_0_move_threshold"].key),
            label="gate 0 move threshold",
        )
        gate_5_read = waiter.expect(
            _number_state_of(numbers["gate_5_move_threshold"].key),
            label="gate 5 move threshold",
        )

        def record(state: EntityState) -> None:
            if isinstance(state, NumberState) and not state.missing_state:
                states.setdefault(state.key, []).append(state.state)
            waiter.on_state(state)

        initial_state_helper = await _wait_initial(client, entities, record)

        # The late answer may already have been parsed when the client connected
        for state in initial_state_helper.initial_states.values():
            record(state)

        try:
            _, gate_0_state, gate_5_state = await asyncio.gather(
                asyncio.wait_for(dropped, timeout=5.0), gate_0_read, gate_5_read
            )
        except TimeoutError:
            pytest.fail(
                "Timeout waiting for the threshold answers.\n"
                f"  ld2412 warnings: {warnings}\n"
                f"  states: {states}"
            )

        assert gate_0_state.state == pytest.approx(10.0)
        assert gate_5_state.state == pytest.approx(15.0)

        for number in numbers.values():
            for value in states.get(number.key, []):
                assert int(value) not in merged_values, (
                    f"{number.object_id} took {value} from the merged frame"
                )
