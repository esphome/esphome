"""Integration tests for the UFM-01 flow meter with a simulated meter on the mock UART.

All tests share uart_mock_ufm01.yaml, whose on_tx lambda answers like a UFM-01: it ACKs commands,
answers sensor data and software version reads, and streams active frames each second after
SET_ACTIVE_MODE. The "Silent" switch stops it answering, "Pause Stream" stops the stream while it still
answers, and "Resume Stream" starts the stream again.

test_uart_mock_ufm01_startup:
  Reset, software version read and active mode go out in order, the active frames publish flow,
  temperature and accumulated flow, and the meter ID and software version publish once.
test_uart_mock_ufm01_stale_recovery:
  A stopped stream marks flow and temperature unknown after 5 s and switches to passive polling,
  whose answers publish; a resumed stream switches back to the active stream and polling stops.
test_uart_mock_ufm01_escalation:
  Eight unanswered passive polls send a reset and run the startup sequence again; values publish
  once the meter answers, and the diagnostics are not published again.
test_uart_mock_ufm01_clear_action:
  The clear command is sent once the UART is idle while the automation continues at once; the
  meter's ACK, or its absence, is only logged, and a second request joins the pending one.
"""

from __future__ import annotations

import asyncio
from collections.abc import Awaitable, Callable
import math
import re

from aioesphomeapi import (
    APIClient,
    ButtonInfo,
    EntityInfo,
    EntityState,
    SensorState,
    SwitchInfo,
    TextSensorState,
    UserService,
)
import pytest

from .state_utils import InitialStateHelper, StateWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

# Commands as the uart mock logs them: FE FE 11, command, argument, checksum, 16
RESET_DEVICE = "FE:FE:11:5D:CB:28:16"
SET_ACTIVE_MODE = "FE:FE:11:5C:00:5C:16"
SET_PASSIVE_MODE = "FE:FE:11:5C:01:5D:16"
READ_SENSOR_DATA = "FE:FE:11:5B:0F:6A:16"
GET_SOFTWARE_VERSION = "FE:FE:11:5E:62:C0:16"
CLEAR_ACCUMULATED_FLOW = "FE:FE:11:5A:FD:57:16"

# Values the fixture's frames encode
ACTIVE_FLOW = 1.2345
ACTIVE_TEMPERATURE = 23.5
ACTIVE_ACCUMULATED = 12.345
PASSIVE_FLOW = 0.567
PASSIVE_TEMPERATURE = 19.25
PASSIVE_ACCUMULATED = 25.678
METER_ID = "2307140001"
SOFTWARE_VERSION = "23380315"

ACTIVE_STARTED = "UFM-01 active stream started"
ACTIVE_RESUMED = "UFM-01 active stream resumed"
STALE = "Active stream stale, switching to passive polling"
ESCALATED = "Passive poll failed repeatedly, re-initializing UFM-01"
CLEAR_ACKED = "Clear accumulated flow acknowledged"
CLEAR_NOT_ACKED = "Clear accumulated flow not acknowledged"

# Startup wait, reset ACK, post-reset wait, then the first frame within the 1 s stream interval
STARTUP_TIMEOUT = 8.0
ACTIVE_STALE_S = 5.0

ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;]*m")


class _Log:
    """Record TX frames and ufm01 log lines with their arrival time, and wait for lines."""

    def __init__(self) -> None:
        self.events: list[tuple[float, str]] = []
        self._awaited: list[tuple[str, asyncio.Future[float]]] = []

    def on_line(self, line: str) -> None:
        line = ANSI_ESCAPE.sub("", line)
        if "uart_mock" in line and "TX " in line and "bytes: " in line:
            text = line.split("bytes: ", 1)[1].strip()
        elif "[ufm01" in line:
            text = line
        else:
            return
        now = asyncio.get_running_loop().time()
        self.events.append((now, text))
        for wanted, future in self._awaited:
            if wanted in text and not future.done():
                future.set_result(now)

    def next(self, text: str) -> asyncio.Future[float]:
        """Return a future for the time of the next event containing ``text``."""
        future: asyncio.Future[float] = asyncio.get_running_loop().create_future()
        self._awaited.append((text, future))
        return future

    @property
    def tx(self) -> list[str]:
        return [text for _, text in self.events if text.startswith("FE:")]

    def index(self, text: str, start: int = 0) -> int:
        return next(
            i
            for i, (_, event) in enumerate(self.events)
            if i >= start and text in event
        )

    def tx_between(self, start: int, end: int) -> list[str]:
        return [text for _, text in self.events[start:end] if text.startswith("FE:")]


class _Meter:
    """Entities and state history of the running fixture."""

    def __init__(
        self, client: APIClient, entities: list[EntityInfo], services: list[UserService]
    ) -> None:
        self.client = client
        self.services = services
        self.by_id = {entity.object_id: entity for entity in entities}
        self.waiter = StateWaiter()
        self.states: dict[str, list[float | str]] = {name: [] for name in self.by_id}
        self._names = {entity.key: entity.object_id for entity in entities}
        self.helper = InitialStateHelper(entities)

    def on_state(self, state: EntityState) -> None:
        if (
            isinstance(state, (SensorState, TextSensorState))
            and not state.missing_state
        ):
            self.states[self._names[state.key]].append(state.state)
        self.waiter.on_state(state)

    def expect(
        self, name: str, check: Callable[[float | str], bool], timeout: float
    ) -> Awaitable[EntityState]:
        key = self.by_id[name].key
        return self.waiter.expect(
            lambda s: s.key == key and not s.missing_state and check(s.state),
            timeout=timeout,
            label=name,
        )

    def press(self, name: str) -> None:
        entity = self.by_id[name]
        assert isinstance(entity, ButtonInfo)
        self.client.button_command(entity.key)

    def set_silent(self, silent: bool) -> None:
        entity = self.by_id["silent"]
        assert isinstance(entity, SwitchInfo)
        self.client.switch_command(entity.key, silent)

    def all_values(self, name: str) -> list[float | str]:
        """Return every value ``name`` held, including one already set at connect time."""
        initial = self.helper.initial_states[self.by_id[name].key]
        return ([] if initial.missing_state else [initial.state]) + self.states[name]


async def _connect(client: APIClient) -> _Meter:
    entities, services = await client.list_entities_services()
    meter = _Meter(client, entities, services)
    client.subscribe_states(meter.helper.on_state_wrapper(meter.on_state))
    try:
        await meter.helper.wait_for_initial_states()
    except TimeoutError:
        pytest.fail("Timeout waiting for initial states")
    return meter


def _is_nan(value: float | str) -> bool:
    return isinstance(value, float) and math.isnan(value)


def _approx(expected: float) -> Callable[[float | str], bool]:
    return lambda value: value == pytest.approx(expected, abs=1e-4)


def _active_values(meter: _Meter, timeout: float) -> list[Awaitable[EntityState]]:
    """Arm waits for the next flow, temperature and accumulated flow from an active frame."""
    return [
        meter.expect("flow", _approx(ACTIVE_FLOW), timeout),
        meter.expect("temperature", _approx(ACTIVE_TEMPERATURE), timeout),
        meter.expect("accumulated_flow", _approx(ACTIVE_ACCUMULATED), timeout),
    ]


async def _gather(meter: _Meter, waits: list[Awaitable[EntityState]]) -> None:
    try:
        await asyncio.gather(*waits)
    except TimeoutError as err:
        pytest.fail(f"{err}; states: {meter.states}")


async def _wait_for(log: _Log, future: asyncio.Future[float], timeout: float) -> float:
    return await wait_or_fail(future, timeout, "Timed out", log=log.events)


@pytest.mark.shared_yaml("uart_mock_ufm01")
@pytest.mark.asyncio
async def test_uart_mock_ufm01_startup(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Startup reaches the active stream, publishes the decoded values and the diagnostics once."""
    log = _Log()
    started = log.next(ACTIVE_STARTED)
    async with (
        run_compiled(yaml_config, line_callback=log.on_line),
        api_client_connected() as client,
    ):
        meter = await _connect(client)
        values = _active_values(meter, STARTUP_TIMEOUT)
        await _wait_for(log, started, STARTUP_TIMEOUT)
        await _gather(meter, values)
        # Two more stream frames
        for _ in range(2):
            await meter.expect("flow", _approx(ACTIVE_FLOW), 2.0)

        assert log.tx == [RESET_DEVICE, GET_SOFTWARE_VERSION, SET_ACTIVE_MODE]
        assert meter.all_values("meter_id") == [METER_ID]
        assert meter.all_values("software_version") == [SOFTWARE_VERSION]


@pytest.mark.shared_yaml("uart_mock_ufm01")
@pytest.mark.asyncio
async def test_uart_mock_ufm01_stale_recovery(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A stopped stream falls back to passive polling and a resumed stream ends it."""
    log = _Log()
    started = log.next(ACTIVE_STARTED)
    async with (
        run_compiled(yaml_config, line_callback=log.on_line),
        api_client_connected() as client,
    ):
        meter = await _connect(client)
        await _wait_for(log, started, STARTUP_TIMEOUT)
        await meter.expect("flow", _approx(ACTIVE_FLOW), 2.0)

        stale = log.next(STALE)
        flow_unknown = meter.expect("flow", _is_nan, ACTIVE_STALE_S + 2)
        temperature_unknown = meter.expect("temperature", _is_nan, ACTIVE_STALE_S + 2)
        polled = [
            meter.expect("flow", _approx(PASSIVE_FLOW), ACTIVE_STALE_S + 4),
            meter.expect(
                "temperature", _approx(PASSIVE_TEMPERATURE), ACTIVE_STALE_S + 4
            ),
            meter.expect(
                "accumulated_flow", _approx(PASSIVE_ACCUMULATED), ACTIVE_STALE_S + 4
            ),
        ]
        meter.press("pause_stream")
        paused_at = asyncio.get_running_loop().time()
        stale_at = await _wait_for(log, stale, ACTIVE_STALE_S + 2)
        # The last frame came up to 1 s before the pause
        assert stale_at - paused_at >= ACTIVE_STALE_S - 1.5
        await _gather(meter, [flow_unknown, temperature_unknown])
        await _gather(meter, polled)

        stale_index = log.index(STALE)
        assert log.tx_between(stale_index, len(log.events))[:2] == [
            SET_PASSIVE_MODE,
            READ_SENSOR_DATA,
        ]
        # Unknown first, then the passive answers
        assert _is_nan(meter.states["flow"][-2])
        assert _is_nan(meter.states["temperature"][-2])

        resumed = log.next(ACTIVE_RESUMED)
        meter.press("resume_stream")
        values = _active_values(meter, 3.0)
        await _wait_for(log, resumed, 3.0)
        await _gather(meter, values)
        # Two more stream frames without any poll in between
        for _ in range(2):
            await meter.expect("flow", _approx(ACTIVE_FLOW), 2.0)
        assert READ_SENSOR_DATA not in log.tx_between(
            log.index(ACTIVE_RESUMED), len(log.events)
        )


@pytest.mark.shared_yaml("uart_mock_ufm01")
@pytest.mark.asyncio
async def test_uart_mock_ufm01_escalation(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Eight unanswered passive polls reset the meter and rerun the startup sequence."""
    log = _Log()
    started = log.next(ACTIVE_STARTED)
    async with (
        run_compiled(yaml_config, line_callback=log.on_line),
        api_client_connected() as client,
    ):
        meter = await _connect(client)
        await _wait_for(log, started, STARTUP_TIMEOUT)
        await meter.expect("flow", _approx(ACTIVE_FLOW), 2.0)

        escalated = log.next(ESCALATED)
        meter.set_silent(True)
        # Stale after 5 s, then eight polls that each time out after 1 s
        flow_count = len(meter.states["flow"])
        await _wait_for(log, escalated, ACTIVE_STALE_S + 8 + 4)

        restarted = log.next(ACTIVE_STARTED)
        values = _active_values(meter, STARTUP_TIMEOUT)
        meter.set_silent(False)
        await _wait_for(log, restarted, STARTUP_TIMEOUT)
        await _gather(meter, values)

        stale_index = log.index(STALE)
        escalated_index = log.index(ESCALATED, stale_index)
        assert (
            log.tx_between(stale_index, escalated_index)
            == [SET_PASSIVE_MODE] + [READ_SENSOR_DATA] * 8
        )
        # A reset (resent once if the meter was still silent), then active mode; the software
        # version is already known, so it is not read again
        after = log.tx_between(
            escalated_index, log.index(ACTIVE_STARTED, escalated_index)
        )
        assert after in (
            [RESET_DEVICE, SET_ACTIVE_MODE],
            [RESET_DEVICE, RESET_DEVICE, SET_ACTIVE_MODE],
        ), after
        # Unknown when the stream went stale and again on the re-init, nothing in between;
        # a frame already in flight when the meter went silent may come first
        flow = meter.states["flow"][flow_count:]
        first_unknown = next(i for i, value in enumerate(flow) if _is_nan(value))
        assert _is_nan(flow[first_unknown + 1]), flow
        assert flow[first_unknown + 2] == pytest.approx(ACTIVE_FLOW, abs=1e-4), flow

        assert meter.all_values("meter_id") == [METER_ID]
        assert meter.all_values("software_version") == [SOFTWARE_VERSION]


@pytest.mark.shared_yaml("uart_mock_ufm01")
@pytest.mark.asyncio
async def test_uart_mock_ufm01_clear_action(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The clear command is sent once the UART is idle; the automation continues at once."""
    log = _Log()
    started = log.next(ACTIVE_STARTED)
    acked = log.next(CLEAR_ACKED)
    async with (
        run_compiled(yaml_config, line_callback=log.on_line),
        api_client_connected() as client,
    ):
        meter = await _connect(client)
        # A second entity listing would re-send the initial states and duplicate the first completion
        (clear,) = meter.services
        # Normally still inside the startup wait; a slow runner may already be streaming
        queued_during_startup = not started.done()

        completed = meter.expect("clear_completions", _approx(1), STARTUP_TIMEOUT)
        zeroed = meter.expect("accumulated_flow", _approx(0.0), STARTUP_TIMEOUT + 2)
        await client.execute_service(clear, {})
        await _wait_for(log, acked, STARTUP_TIMEOUT)
        await _gather(meter, [completed, zeroed])

        # Queued during startup, sent only once the active stream started
        if queued_during_startup:
            assert CLEAR_ACCUMULATED_FLOW not in log.tx_between(
                0, log.index(ACTIVE_STARTED)
            )
        assert log.tx.count(CLEAR_ACCUMULATED_FLOW) == 1
        assert log.index(CLEAR_ACCUMULATED_FLOW) < log.index(CLEAR_ACKED)

        # Two requests in a row share one command; without an answer it is reported after the 500 ms ACK timeout
        sent = log.next(CLEAR_ACCUMULATED_FLOW)
        not_acked = log.next(CLEAR_NOT_ACKED)
        completed = meter.expect("clear_completions", _approx(3), 3.0)
        meter.set_silent(True)
        await asyncio.sleep(0.2)
        await client.execute_service(clear, {})
        await client.execute_service(clear, {})
        sent_at = await _wait_for(log, sent, 2.0)
        not_acked_at = await _wait_for(log, not_acked, 2.0)
        await _gather(meter, [completed])
        assert not_acked_at - sent_at >= 0.4
        assert log.tx.count(CLEAR_ACCUMULATED_FLOW) == 2
        assert meter.states["clear_completions"] == [1.0, 2.0, 3.0]
