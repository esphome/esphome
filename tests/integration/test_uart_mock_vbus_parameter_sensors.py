"""P2 sensor entities publish confirmed, scaled values over the host API alongside P1 RX."""

import asyncio
import re

from aioesphomeapi import ButtonInfo, SensorInfo, SensorState
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_vbus_parameter_sensors(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    results: list[str] = []
    delayed: list[str] = []
    writes: list[str] = []
    readings: list[str] = []
    values: list[str] = []
    entity_values: list[str] = []
    api_states: dict[int, float] = {}
    complete = asyncio.Event()

    def on_line(line: str) -> None:
        text = re.sub(r"\x1b\[[0-9;]*m", "", line).rsplit(": ", 1)[-1].strip()
        if "[vbus.result_test:" in line:
            results.append(text)
        if "[vbus.entity_test:" in line:
            entity_values.append(text)
        if "[vbus.value_test:" in line:
            values.append(text)
        if "[vbus.delayed_test:" in line:
            delayed.append(text)
        if "[vbus.wire_test:" in line:
            writes.append(text.removesuffix(" (16)"))
        if "[vbus.rx_test:" in line and "P1=1" in line:
            readings.append(text)
            if len(readings) == 2:
                complete.set()

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        start = find_entity(entities, "start_scenario", ButtonInfo)
        assert start is not None
        signed = find_entity(entities, "signed_parameter", SensorInfo)
        unsigned = find_entity(entities, "unsigned_parameter", SensorInfo)
        assert signed is not None
        assert unsigned is not None

        def on_state(state: SensorState) -> None:
            if isinstance(state, SensorState):
                api_states[state.key] = state.state

        client.subscribe_states(on_state)
        client.button_command(start.key)
        await asyncio.wait_for(complete.wait(), timeout=5.0)
        await asyncio.sleep(0.5)
        assert results == ["512 2 456 0", "768 1 123 0", "768 2 456 0", "768 1 123 0"]
        assert values == ["2 456", "1 123"]
        assert delayed == values
        assert entity_values == ["unsigned=456", "signed=12.3"]
        assert api_states[signed.key] == pytest.approx(12.3)
        assert api_states[unsigned.key] == 456
        assert len(readings) == 2
        assert len(writes) == 7
        assert writes[0] == "AA.11.22.20.00.20.00.02.02.00.48.01.00.00.04.3B"
        assert writes[1] == "AA.11.22.20.00.20.00.03.02.00.00.00.00.00.00.07"
        assert writes[2] == "AA.11.22.20.00.20.00.03.01.00.00.00.00.00.00.08"
        assert writes[3] == "AA.11.22.20.00.20.00.06.00.00.00.00.00.00.00.06"
        assert writes[4] == writes[1]
        assert writes[5] == writes[2]
        assert writes[6] == writes[3]
