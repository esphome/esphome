"""Native API clock controls and readable write results share the existing bus."""

import asyncio
import re

from aioesphomeapi import (
    ButtonInfo,
    EntityState,
    TextSensorInfo,
    TextSensorState,
    TimeInfo,
    TimeState,
)
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
@pytest.mark.shared_yaml("uart_mock_vbus_setting_write")
async def test_uart_mock_vbus_ux(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    initial = asyncio.Event()
    unchanged = asyncio.Event()
    repeated = asyncio.Event()
    clock_states: list[tuple[int, int, int]] = []
    result_states: list[str] = []
    writes: list[bytes] = []
    no_ops = 0
    raw_results = 0

    def on_line(line: str) -> None:
        nonlocal no_ops, raw_results
        if "[vbus.result_raw:" in line and "already set; no write needed" in line:
            raw_results += 1
        if "[vbus.write_test:" in line:
            text = re.sub(r"\x1b\[[0-9;]*m", "", line).rsplit(": ", 1)[-1].strip()
            if [int(value) for value in text.split()][:2] == [2, 4]:
                no_ops += 1
                if no_ops == 2:
                    repeated.set()
        if "[vbus.wire_test:" not in line:
            return
        text = re.sub(r"\x1b\[[0-9;]*m", "", line).rsplit(": ", 1)[-1].strip()
        frame = bytes.fromhex(text.removesuffix(" (16)").replace(".", " "))
        if frame[7] == 2:
            writes.append(frame)

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        start = find_entity(entities, "start_scenario", ButtonInfo)
        clock = find_entity(entities, "controller_clock", TimeInfo)
        result = find_entity(entities, "last_setting_write", TextSensorInfo)
        assert start is not None and clock is not None and result is not None

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, TimeState)
                and state.key == clock.key
                and not state.missing_state
            ):
                clock_states.append((state.hour, state.minute, state.second))
                initial.set()
            elif isinstance(state, TextSensorState) and state.key == result.key:
                if (
                    state.state
                    == "SETTING 2: ALREADY SET; NO WRITE NEEDED (VALUE 62, RAW 62)"
                ):
                    result_states.append(state.state)
                    unchanged.set()

        client.subscribe_states(on_state)
        client.button_command(start.key)
        await asyncio.wait_for(initial.wait(), timeout=12)
        assert clock_states == [(1, 2, 0)]
        client.time_command(clock.key, 1, 2, 0)
        await asyncio.wait_for(unchanged.wait(), timeout=5)
        client.time_command(clock.key, 1, 2, 0)
        await asyncio.wait_for(repeated.wait(), timeout=5)
        client.time_command(clock.key, 1, 3, 1)
        await asyncio.sleep(0.1)
        assert clock_states == [(1, 2, 0)]
        assert len(result_states) == 1
        assert raw_results == 1
        assert not writes


@pytest.mark.asyncio
@pytest.mark.shared_yaml("uart_mock_vbus_setting_write")
@pytest.mark.parametrize(
    "entity,scheduled,hour,minute,wire_value",
    [
        ("controller_clock", False, 7, 30, bytes([2, 0, 66, 1, 0, 0, 4])),
        ("thermostat_window_start", True, 6, 15, bytes([3, 14, 119, 1, 0, 0, 0])),
    ],
)
async def test_uart_mock_vbus_clock_write_round_trip(
    entity: str,
    scheduled: bool,
    hour: int,
    minute: int,
    wire_value: bytes,
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    initial = asyncio.Event()
    verified = asyncio.Event()
    writes: list[bytes] = []

    def on_line(line: str) -> None:
        if "[vbus.wire_test:" in line:
            text = re.sub(r"\x1b\[[0-9;]*m", "", line).rsplit(": ", 1)[-1].strip()
            frame = bytes.fromhex(text.removesuffix(" (16)").replace(".", " "))
            if frame[7] == 2:
                writes.append(frame)

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        start = find_entity(entities, "start_scenario", ButtonInfo)
        scheme = find_entity(entities, "use_thermostat_scheme", ButtonInfo)
        clock = find_entity(entities, entity, TimeInfo)
        assert start is not None and scheme is not None and clock is not None

        def on_state(state: EntityState) -> None:
            if (
                isinstance(state, TimeState)
                and state.key == clock.key
                and not state.missing_state
            ):
                initial.set()
                if (state.hour, state.minute, state.second) == (hour, minute, 0):
                    verified.set()

        client.subscribe_states(on_state)
        if scheduled:
            client.button_command(scheme.key)
        client.button_command(start.key)
        await asyncio.wait_for(initial.wait(), timeout=12)
        for invalid in [(25, 30, 0), (7, 60, 0), (7, 30, 60), (7, 30, 1)]:
            client.time_command(clock.key, *invalid)
        if scheduled:
            client.time_command(clock.key, 6, 7, 0)
        await asyncio.sleep(0.1)
        assert not writes
        client.time_command(clock.key, hour, minute, 0)
        await asyncio.wait_for(verified.wait(), timeout=5)
        assert len(writes) == 1
        assert writes[0][8:15] == wire_value
        assert sum(writes[0][1:]) & 0x7F == 0x7F
