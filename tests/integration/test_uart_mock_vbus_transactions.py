"""Read, verified write, uncertain write timeout and late ACKs use only a host mock."""

import asyncio
import re

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_vbus_transactions(
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
    complete = asyncio.Event()

    def on_line(line: str) -> None:
        text = re.sub(r"\x1b\[[0-9;]*m", "", line).rsplit(": ", 1)[-1].strip()
        if "[vbus.result_test:" in line:
            results.append(text)
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
        client.button_command(start.key)
        await asyncio.wait_for(complete.wait(), timeout=5.0)
        await asyncio.sleep(0.5)
        assert results == ["768 1 123 0", "512 2 456 0", "512 4 0 1"]
        assert delayed == results
        assert len(readings) == 2
        assert len(writes) == 6
        assert writes[0] == "AA.11.22.20.00.20.00.03.01.00.00.00.00.00.00.08"
        assert writes[1] == "AA.11.22.20.00.20.00.02.02.00.48.01.00.00.04.3B"
        assert writes[2] == "AA.11.22.20.00.20.00.03.02.00.00.00.00.00.00.07"
        assert writes[3] == "AA.11.22.20.00.20.00.06.00.00.00.00.00.00.00.06"
        assert writes[4] == "AA.11.22.20.00.20.00.02.04.00.15.03.00.00.00.6E"
        assert writes[5] == writes[3]
