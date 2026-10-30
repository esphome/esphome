"""Suspension cancels in-flight TX while RX continues, then resumes fresh reads only."""

import asyncio
import re

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_vbus_suspend_polling(
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
    suspended: list[str] = []
    events: list[str] = []
    complete = asyncio.Event()

    def on_line(line: str) -> None:
        text = re.sub(r"\x1b\[[0-9;]*m", "", line).rsplit(": ", 1)[-1].strip()
        if "[vbus.suspend_test:" in line:
            suspended.append(text)
        if "[vbus.result_test:" in line:
            results.append(text)
        if "[vbus.value_test:" in line:
            values.append(text)
        if "[vbus.delayed_test:" in line:
            delayed.append(text)
        if "[vbus.wire_test:" in line:
            writes.append(text.removesuffix(" (16)"))
            events.append(text.removesuffix(" (16)"))
        if "[vbus.rx_test:" in line and "P1=1" in line:
            readings.append(text)
            events.append("RX")
            if len(readings) == 2:
                complete.set()

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        start = find_entity(entities, "start_scenario", ButtonInfo)
        assert start is not None
        client.button_command(start.key)
        await asyncio.wait_for(complete.wait(), timeout=5.0)
        await asyncio.sleep(0.5)
        assert results == ["512 9 0 4", "768 1 123 0", "768 2 456 0"]
        assert values == ["1 123", "2 456"]
        assert delayed == values
        assert suspended == ["write accepted while suspended=0"]
        assert len(readings) == 2
        assert writes == [
            "AA.11.22.20.00.20.00.02.09.00.63.00.00.00.00.1E",
            "AA.11.22.20.00.20.00.03.01.00.00.00.00.00.00.08",
            "AA.11.22.20.00.20.00.03.02.00.00.00.00.00.00.07",
            "AA.11.22.20.00.20.00.06.00.00.00.00.00.00.00.06",
        ]
        assert events.index("RX") < events.index(writes[1])
