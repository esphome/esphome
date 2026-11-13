"""The CS Plus controller commits a complete scan after wire release and resumed P1 RX."""

import asyncio
import re

from aioesphomeapi import ButtonInfo, SensorInfo, SensorState
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_vbus_controller(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    frames: list[bytes] = []
    snapshots: list[list[int]] = []
    states: dict[int, float] = {}
    complete = asyncio.Event()

    def on_line(line: str) -> None:
        text = re.sub(r"\x1b\[[0-9;]*m", "", line).rsplit(": ", 1)[-1].strip()
        if "[vbus.wire_test:" in line:
            frames.append(bytes.fromhex(text.removesuffix(" (16)").replace(".", " ")))
        if "[vbus.snapshot_test:" in line:
            snapshots.append([int(value) for value in text.split()])
            complete.set()

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        start = find_entity(entities, "start_scenario", ButtonInfo)
        rx = find_entity(entities, "controller_rx", SensorInfo)
        assert start is not None
        assert rx is not None

        def on_state(state: SensorState) -> None:
            if isinstance(state, SensorState):
                states[state.key] = state.state

        client.subscribe_states(on_state)
        client.button_command(start.key)
        await asyncio.wait_for(complete.wait(), timeout=12.0)
        await asyncio.sleep(0.2)
        assert len(snapshots) == 1
        completed, reads, groups, releases, scheme, unit, language = snapshots[0]
        assert completed == 1
        assert scheme == 1  # P1 System is deliberately 9.
        assert unit == 0  # P1 UnitType is deliberately 1.
        assert language == 4
        assert groups == releases
        assert states[rx.key] == pytest.approx(12.3)
        ids: list[int] = []
        group_reads = 0
        for frame in frames:
            assert len(frame) == 16
            assert frame[:8] in [
                bytes.fromhex("AA11222000200003"),
                bytes.fromhex("AA11222000200006"),
            ]
            assert sum(frame[1:]) & 0x7F == 0x7F
            if frame[7] == 6:
                assert 1 <= group_reads <= 16
                group_reads = 0
                continue
            group_reads += 1
            index = (frame[8] | ((frame[14] & 1) << 7)) | (
                (frame[9] | ((frame[14] & 2) << 6)) << 8
            )
            ids.append(index)
        assert ids[:2] == [1, 10]
        assert len(ids) == len(set(ids)) == reads
        assert 2 not in ids  # Clock comes from P1.
        assert 2771 not in ids  # Drainback is disabled.
        assert group_reads == 0
