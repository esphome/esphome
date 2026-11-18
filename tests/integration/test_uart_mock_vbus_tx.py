"""Optional TX waits for a valid grant, releases once and retains P1 reception."""

import asyncio

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_vbus_tx(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setenv("ESPHOME_PCH_ENABLE", "0")
    writes: list[str] = []
    grants: list[str] = []
    readings: list[str] = []
    rejected: list[str] = []
    replies: list[str] = []
    received = asyncio.Event()

    def on_line(line: str) -> None:
        if "[vbus.tx_test:" in line:
            writes.append(line)
        if "VBus test grant acquired" in line:
            grants.append(line)
        if "[vbus.rx_test:" in line and "P1=1" in line:
            readings.append(line)
            if len(readings) == 2:
                received.set()
        if "Datagram not sent" in line:
            rejected.append(line)
        if "[vbus.reply_test:" in line:
            replies.append(line)

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        start = find_entity(entities, "start_scenario", ButtonInfo)
        assert start is not None
        client.button_command(start.key)
        await asyncio.sleep(0.2)
        assert not writes
        assert not grants
        await asyncio.wait_for(received.wait(), timeout=5.0)
        await asyncio.sleep(0.6)
        assert len(grants) == 2
        assert len(rejected) == 2
        assert len(replies) == 2
        assert all("value=123" in line for line in replies)
        assert len(writes) == 4
        assert "AA.11.22.20.00.20.00.03.02.00.00.00.00.00.00.07" in writes[0]
        assert "AA.11.22.20.00.20.00.06.00.00.00.00.00.00.00.06" in writes[1]
        assert "AA.11.22.20.00.20.00.03.02.00.00.00.00.00.00.07" in writes[2]
        assert "AA.11.22.20.00.20.00.06.00.00.00.00.00.00.00.06" in writes[3]
