"""Integration test for LD2412 component with mock UART.

Tests:
test_uart_mock_ld2412 (normal mode):
  1. Happy path - valid data frame publishes correct sensor values
  2. Garbage resilience - random bytes don't crash the component
  3. Truncated frame handling - partial frame doesn't corrupt state
  4. Truncated frame recovery - the frame is dropped once its length is reached
  5. Post-garbage parsing - the next valid frame is parsed correctly
  6. Footer bytes in the values - a frame whose values spell the command footer is parsed whole
  7. Replay after a drop - a valid frame caught inside a dropped frame's window is still parsed
  8. Impossible counts - a length field of 0xFFF6 or 0xFFFF is dropped and the next frame is parsed
  9. TX logging - verifies LD2412 sends expected setup commands

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

The gate threshold tests (footer_in_payload, truncated_answer, requery_after_giveup) cover
answers that hold the frame footer or are truncated, writes that fill the gates without a
number from the module's values, and writes held back until the module reports them.
test_uart_mock_ld2412_short_answers checks that a version or basic configuration answer cut
short publishes nothing, while the whole answer after it does.
"""

from __future__ import annotations

import asyncio
from collections.abc import Callable
import math

from aioesphomeapi import (
    APIClient,
    ButtonInfo,
    EntityInfo,
    EntityState,
    NumberInfo,
    NumberState,
    TextSensorState,
)
import pytest

from .state_utils import (
    InitialStateHelper,
    SensorStateCollector,
    StateWaiter,
    require_entity,
)
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

# Length and command word of each frame as the uart mock logs it; the payload is logged on its own TX line
MOTION_COMMAND = "10:00:03:00"
STILL_COMMAND = "10:00:04:00"
MOTION_QUERY = "02:00:13:00"
STILL_QUERY = "02:00:14:00"
CONFIG_MODE_LEFT = "02:00:FE:00"
FACTORY_RESET = "02:00:A2:00"
# Logged in place of the motion command when the write is held back or given up
MOTION_HELD_BACK = "Command 03 held back"
MOTION_GIVEN_UP = "Command 03 not sent, the module did not report"

THRESHOLD_NUMBERS = (
    "gate_0_move_threshold",
    "gate_0_still_threshold",
    "gate_5_move_threshold",
    "gate_5_still_threshold",
)


def _is_tx(line: str) -> bool:
    return "uart_mock" in line and "TX " in line


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
    await initial_state_helper.wait_for_initial_states()
    return initial_state_helper


def _number_state_of(key: int) -> Callable[[EntityState], bool]:
    """Return a predicate that matches a number state with a value for ``key``."""
    return lambda s: isinstance(s, NumberState) and not s.missing_state and s.key == key


def _is_nan_of(key: int) -> Callable[[EntityState], bool]:
    """Return a predicate that matches the number ``key`` being marked unknown."""
    return lambda s: _number_state_of(key)(s) and math.isnan(s.state)


class _LogEvents:
    """Record the TX lines and the log lines holding one of ``texts``, in the order they were logged."""

    def __init__(self, *texts: str) -> None:
        self._texts = texts
        self.lines: list[str] = []
        self._awaited: dict[str, asyncio.Future[None]] = {}

    def on_line(self, line: str) -> None:
        if _is_tx(line) or any(text in line for text in self._texts):
            self.lines.append(line)
        for text, future in self._awaited.items():
            if text in line and not future.done():
                future.set_result(None)

    def next_line_with(self, text: str) -> asyncio.Future[None]:
        future: asyncio.Future[None] = asyncio.get_running_loop().create_future()
        self._awaited[text] = future
        return future

    def index_after(self, start: int, text: str) -> int:
        return next(
            i for i, line in enumerate(self.lines) if i > start and text in line
        )

    def count(self, text: str) -> int:
        return sum(text in line for line in self.lines)


@pytest.mark.asyncio
async def test_uart_mock_ld2412(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test LD2412 data parsing with happy path, garbage, a truncated frame, and recovery."""
    loop = asyncio.get_running_loop()

    # Track the warning for the dropped truncated frame in logs
    resync_seen = loop.create_future()

    # Track TX data logged by the mock for assertions
    tx_log_lines: list[str] = []

    def line_callback(line: str) -> None:
        if "Invalid length" in line and not resync_seen.done():
            resync_seen.set_result(True)
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
            and pytest.approx(772.0) in collector.sensor_states["moving_distance"]
            and pytest.approx(400.0) in collector.sensor_states["moving_distance"]
            # Every value asserted below, so no state can still be in flight when the waiter fires
            and pytest.approx(2.0) in collector.sensor_states["moving_energy"]
            and pytest.approx(1.0) in collector.sensor_states["still_distance"]
            and pytest.approx(5.0) in collector.sensor_states["still_energy"]
            and pytest.approx(772.0) in collector.sensor_states["detection_distance"]
            and pytest.approx(7.0) in collector.sensor_states["moving_energy"]
            and pytest.approx(200.0) in collector.sensor_states["still_distance"]
            and pytest.approx(9.0) in collector.sensor_states["still_energy"]
            and pytest.approx(400.0) in collector.sensor_states["detection_distance"]
            and pytest.approx(600.0) in collector.sensor_states["moving_distance"]
            and pytest.approx(11.0) in collector.sensor_states["moving_energy"]
            and pytest.approx(300.0) in collector.sensor_states["still_distance"]
            and pytest.approx(13.0) in collector.sensor_states["still_energy"]
            and pytest.approx(600.0) in collector.sensor_states["detection_distance"]
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
        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)

        # Wait for Phase 1 - all sensors and binary sensors have at least one value
        await collector.wait_for_all(timeout=3.0)

        # Phase 1 values: moving=100, still=120, energy=50/25, detect=100
        assert collector.sensor_states["moving_distance"][0] == pytest.approx(100.0)
        assert collector.sensor_states["still_distance"][0] == pytest.approx(120.0)
        assert collector.sensor_states["moving_energy"][0] == pytest.approx(50.0)
        assert collector.sensor_states["still_energy"][0] == pytest.approx(25.0)
        assert collector.sensor_states["detection_distance"][0] == pytest.approx(100.0)

        # Wait for the recovery frame (Phase 5) to be parsed
        # This proves the component survived garbage + truncated frame + filler bytes
        await wait_or_fail(
            recovery_received,
            15.0,
            "Timeout waiting for recovery frame",
            sensor_states=collector.sensor_states,
        )

        # Verify the truncated frame was dropped with a warning
        assert resync_seen.done(), "Expected 'Invalid length' warning in logs"

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

        # Phase 6: the values 04 03 02 01 did not end the frame early
        assert pytest.approx(772.0) in collector.sensor_states["moving_distance"]
        assert pytest.approx(2.0) in collector.sensor_states["moving_energy"]
        assert pytest.approx(1.0) in collector.sensor_states["still_distance"]
        assert pytest.approx(5.0) in collector.sensor_states["still_energy"]
        assert pytest.approx(772.0) in collector.sensor_states["detection_distance"]

        # Phase 7: the valid frame inside the dropped frame's window was replayed and parsed
        assert pytest.approx(400.0) in collector.sensor_states["moving_distance"]
        assert pytest.approx(7.0) in collector.sensor_states["moving_energy"]
        assert pytest.approx(200.0) in collector.sensor_states["still_distance"]
        assert pytest.approx(9.0) in collector.sensor_states["still_energy"]
        assert pytest.approx(400.0) in collector.sensor_states["detection_distance"]

        # Phase 8: both impossible counts were dropped and the frame after them was parsed
        assert pytest.approx(600.0) in collector.sensor_states["moving_distance"]
        assert pytest.approx(11.0) in collector.sensor_states["moving_energy"]
        assert pytest.approx(300.0) in collector.sensor_states["still_distance"]
        assert pytest.approx(13.0) in collector.sensor_states["still_energy"]
        assert pytest.approx(600.0) in collector.sensor_states["detection_distance"]

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
        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)

        # Wait for Phase 1 - all sensors and binary sensors have at least one value
        await collector.wait_for_all(timeout=3.0)

        # Phase 1 values (engineering mode frame):
        # moving=30, energy=100, still=30, energy=100, detect=30
        assert collector.sensor_states["moving_distance"][0] == pytest.approx(30.0)
        assert collector.sensor_states["still_distance"][0] == pytest.approx(30.0)
        assert collector.sensor_states["gate_0_move_energy"][0] == pytest.approx(100.0)
        assert collector.sensor_states["gate_1_move_energy"][0] == pytest.approx(65.0)
        assert collector.sensor_states["light"][0] == pytest.approx(87.0)

        # Wait for Phase 3 frame: still_distance = 291cm (multi-byte)
        await wait_or_fail(
            phase3_still_received,
            3.0,
            "Timeout waiting for Phase 3 still_distance",
            still_distance=collector.sensor_states["still_distance"],
        )

        assert pytest.approx(291.0) in collector.sensor_states["still_distance"]

        # Wait for Phase 3: detection_distance = 291 (still-only target)
        await wait_or_fail(
            phase3_detect_received,
            3.0,
            "Timeout waiting for detection_distance=291",
            detection_distance=collector.sensor_states["detection_distance"],
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

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)

        # Wait for Phase 1 — valid engineering frame establishes baseline
        await collector.wait_for_all(timeout=3.0)

        # Phase 1 baseline: gate_0_move=100, light=87
        assert collector.sensor_states["gate_0_move_energy"][0] == pytest.approx(100.0)
        assert collector.sensor_states["light"][0] == pytest.approx(87.0)

        # Wait for Phase 3 recovery frame (gate_0_move=50)
        await wait_or_fail(
            recovery_received,
            3.0,
            "Timeout waiting for recovery frame",
            gate_0_move_energy=collector.sensor_states["gate_0_move_energy"],
            light=collector.sensor_states["light"],
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
async def test_uart_mock_ld2412_footer_in_payload(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test threshold answers that hold the frame footer 04 03 02 01, and the writes built from them."""
    # Gate 0 set to 77 (0x4D); the other 13 gates must match the motion answer byte for byte
    expected_motion = "4D:23:23:23:23:04:03:02:01:19:19:19:19:19"
    # Gate 5 set to 77, the rest matches the still answer
    expected_still = "00:23:23:23:23:4D:19:19:19:19:04:03:02:01"
    # Gate 0 set to 50 (0x32) after the reset
    expected_motion_after_reset = "32:23:23:23:23:04:03:02:01:19:19:19:19:19"
    events = _LogEvents(MOTION_HELD_BACK)

    async with (
        run_compiled(yaml_config, line_callback=events.on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        initial_state_helper = await _wait_initial(client, entities)
        numbers = _threshold_numbers(entities)

        # Gate 5 move is the first footer byte inside the motion answer
        for name, value in {
            "gate_0_move_threshold": 0.0,
            "gate_5_move_threshold": 4.0,
            "gate_0_still_threshold": 0.0,
            "gate_5_still_threshold": 25.0,
        }.items():
            state = initial_state_helper.initial_states[numbers[name].key]
            assert state.state == pytest.approx(value), name

        motion_written = events.next_line_with(expected_motion)
        client.number_command(numbers["gate_0_move_threshold"].key, 77.0)
        await wait_or_fail(
            motion_written,
            5.0,
            "Timeout waiting for the motion threshold command",
            expected_payload=expected_motion,
        )

        assert events.count(STILL_COMMAND) == 0, (
            "still threshold command was sent for a motion write"
        )

        still_written = events.next_line_with(expected_still)
        client.number_command(numbers["gate_5_still_threshold"].key, 77.0)
        await wait_or_fail(
            still_written,
            5.0,
            "Timeout waiting for the still threshold command",
            expected_payload=expected_still,
        )

        assert events.count(MOTION_COMMAND) == 1
        assert events.count(STILL_COMMAND) == 1
        assert events.count(MOTION_HELD_BACK) == 0

        # The reset discards the module's thresholds, so the next write is held back and re-queried
        reset_sent = events.next_line_with(FACTORY_RESET)
        client.button_command(require_entity(entities, "factory_reset", ButtonInfo).key)
        await wait_or_fail(
            reset_sent, 5.0, "Timeout waiting for the factory reset command"
        )

        held_back = events.next_line_with(MOTION_HELD_BACK)
        written_after_reset = events.next_line_with(expected_motion_after_reset)
        client.number_command(numbers["gate_0_move_threshold"].key, 50.0)
        await wait_or_fail(
            asyncio.gather(held_back, written_after_reset),
            5.0,
            "Timeout waiting for the write after the factory reset",
            expected_payload=expected_motion_after_reset,
            logged=events.lines,
        )
        reset_at = events.index_after(-1, FACTORY_RESET)
        warning_at = events.index_after(reset_at, MOTION_HELD_BACK)
        query_at = events.index_after(warning_at, MOTION_QUERY)
        events.index_after(query_at, expected_motion_after_reset)

        entities_after, _ = await client.list_entities_services()
        assert len(entities_after) == len(entities)


@pytest.mark.asyncio
async def test_uart_mock_ld2412_truncated_answer(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that a threshold answer merged with the next frame is dropped and the parser recovers."""
    loop = asyncio.get_running_loop()

    dropped = loop.create_future()
    warnings: list[str] = []

    def line_callback(line: str) -> None:
        if "[W][ld2412" in line:
            warnings.append(line)
        if "Invalid length" in line and not dropped.done():
            dropped.set_result(True)

    # Gate bytes of the truncated answer
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


@pytest.mark.asyncio
async def test_uart_mock_ld2412_requery_after_giveup(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test a write held back until the module reports its thresholds: given up once, then sent."""
    # Gate 0 takes the module value 25 (0x19), gate 5 the user value 30 (0x1E)
    expected_motion = "19:0B:0C:0D:0E:1E:10:11:12:13:14:15:16:17"
    events = _LogEvents(MOTION_HELD_BACK, MOTION_GIVEN_UP)

    async with (
        run_compiled(yaml_config, line_callback=events.on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        numbers = _threshold_numbers(entities)
        gate_0_key = numbers["gate_0_move_threshold"].key
        gate_5_key = numbers["gate_5_move_threshold"].key

        gate_5_values: list[float] = []
        waiter = StateWaiter()

        def on_state(state: EntityState) -> None:
            if _number_state_of(gate_5_key)(state):
                gate_5_values.append(state.state)
            waiter.on_state(state)

        initial_state_helper = await _wait_initial(client, entities, on_state)

        for key in (gate_0_key, gate_5_key):
            assert initial_state_helper.initial_states[key].missing_state

        held_back = events.next_line_with(MOTION_HELD_BACK)
        config_mode_left = events.next_line_with(CONFIG_MODE_LEFT)
        given_up = events.next_line_with(MOTION_GIVEN_UP)
        unknown = [
            waiter.expect(_is_nan_of(key), label="NaN")
            for key in (gate_0_key, gate_5_key)
        ]
        client.number_command(gate_5_key, 20.0)
        await wait_or_fail(
            asyncio.gather(held_back, config_mode_left),
            5.0,
            "Timeout waiting for the write to be handled",
            logged=events.lines,
        )
        # The unanswered query times out after 2 s
        try:
            await asyncio.wait_for(given_up, timeout=3.0)
            await asyncio.gather(*unknown)
        except TimeoutError as err:
            pytest.fail(f"Timeout waiting for the first write to be given up: {err}")

        warning_at = events.index_after(-1, MOTION_HELD_BACK)
        left_at = events.index_after(warning_at, CONFIG_MODE_LEFT)
        write_window = events.lines[warning_at:left_at]
        assert any(MOTION_QUERY in line for line in write_window), (
            "motion thresholds were not queried again after the command was held back"
        )
        assert not any(STILL_QUERY in line for line in write_window), (
            "still thresholds were queried for a motion write"
        )
        assert events.count(MOTION_COMMAND) == 0, (
            "motion threshold command was sent without a value for the gates that have no number"
        )

        held_back_again = events.next_line_with(MOTION_HELD_BACK)
        motion_written = events.next_line_with(expected_motion)
        gate_0_refilled = waiter.expect(
            lambda s: _number_state_of(gate_0_key)(s) and not math.isnan(s.state),
            label="gate 0 move threshold from the module",
        )
        gate_5_values.clear()
        client.number_command(gate_5_key, 30.0)
        try:
            await asyncio.wait_for(
                asyncio.gather(held_back_again, motion_written), timeout=5.0
            )
            gate_0_state = await gate_0_refilled
        except TimeoutError as err:
            pytest.fail(
                f"Timeout waiting for the second write: {err}\n"
                f"  expected motion payload: {expected_motion}\n"
                f"  logged: {events.lines}"
            )

        warning_at = events.index_after(left_at, MOTION_HELD_BACK)
        query_at = events.index_after(warning_at, MOTION_QUERY)
        write_at = events.index_after(query_at, expected_motion)
        assert MOTION_COMMAND in events.lines[write_at - 1]

        # One query each from setup, the given-up write and the second write
        assert events.count(MOTION_QUERY) == 3
        assert events.count(STILL_COMMAND) == 0, (
            "still threshold command was sent for a motion write"
        )

        assert gate_0_state.state == pytest.approx(25.0)
        assert gate_5_values
        assert all(value == pytest.approx(30.0) for value in gate_5_values)


@pytest.mark.asyncio
async def test_uart_mock_ld2412_short_answers(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that answers cut short publish nothing and the whole answers after them do."""
    loop = asyncio.get_running_loop()
    # Number object_id: the value from the whole basic configuration answer
    expected = {"min_distance_gate": 3.0, "max_distance_gate": 8.0, "timeout": 10.0}
    number_states: dict[str, list[float]] = {name: [] for name in expected}
    version_states: list[str] = []
    key_to_name: dict[int, str] = {}
    all_received = loop.create_future()

    def on_state(state: EntityState) -> None:
        if isinstance(state, TextSensorState) and not state.missing_state:
            version_states.append(state.state)
        elif (
            isinstance(state, NumberState)
            and not state.missing_state
            and (name := key_to_name.get(state.key)) is not None
        ):
            number_states[name].append(state.state)
        if not all_received.done() and version_states and all(number_states.values()):
            all_received.set_result(True)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        key_to_name.update(
            {require_entity(entities, name, NumberInfo).key: name for name in expected}
        )
        await _wait_initial(client, entities, on_state)

        start_btn = require_entity(entities, "start_scenario", ButtonInfo)
        client.button_command(start_btn.key)

        await wait_or_fail(
            all_received,
            5.0,
            "Timeout waiting for the whole answers",
            number_states=number_states,
            version_states=version_states,
        )
        # Each entity saw exactly one value, the one from the whole answer
        assert version_states == ["2.07.22062416"]
        for name, value in expected.items():
            assert number_states[name] == pytest.approx([value]), name
