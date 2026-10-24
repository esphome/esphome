"""Explicit API actions exercise preflight, one wire write, readback and an unchanged no-op."""

import asyncio
import re

from aioesphomeapi import ButtonInfo, SensorInfo, SensorState
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_vbus_setting_write(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    frames: list[bytes] = []
    results: list[list[int]] = []
    states: dict[int, float] = {}
    snapshot = asyncio.Event()
    completed = asyncio.Event()

    def on_line(line: str) -> None:
        text = re.sub(r"\x1b\[[0-9;]*m", "", line).rsplit(": ", 1)[-1].strip()
        if "[vbus.wire_test:" in line:
            frames.append(bytes.fromhex(text.removesuffix(" (16)").replace(".", " ")))
        if "[vbus.snapshot_test:" in line:
            snapshot.set()
        if "[vbus.write_test:" in line:
            results.append([int(value) for value in text.split()])
            completed.set()

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        start = find_entity(entities, "start_scenario", ButtonInfo)
        change = find_entity(entities, "set_pump_limit", ButtonInfo)
        rx = find_entity(entities, "controller_rx", SensorInfo)
        assert start is not None and change is not None and rx is not None

        def on_state(state: SensorState) -> None:
            if isinstance(state, SensorState):
                states[state.key] = state.state

        client.subscribe_states(on_state)
        client.button_command(start.key)
        await asyncio.wait_for(snapshot.wait(), timeout=12)
        assert not any(frame[7] == 2 for frame in frames)
        client.button_command(change.key)
        await asyncio.wait_for(completed.wait(), timeout=5)
        assert results == [
            [48, 3, 1, 1]
        ]  # SUCCESS after preflight, readback, release and P1.
        completed.clear()
        client.button_command(change.key)
        await asyncio.wait_for(completed.wait(), timeout=5)
        assert results[-1] == [48, 4, 1, 1]  # UNCHANGED: no second write.
        assert states[rx.key] == pytest.approx(12.3)
        writes = [frame for frame in frames if frame[7] == 2]
        assert len(writes) == 1
        assert writes[0][8:14] == bytes([48, 0, 60, 0, 0, 0])
        for frame in frames:
            assert frame[7] in (2, 3, 6)
            assert sum(frame[1:]) & 0x7F == 0x7F
