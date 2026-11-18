"""P2 RX callbacks reject malformed datagrams and preserve delayed action arguments."""

import asyncio
from pathlib import Path
import re

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import find_entity
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uart_mock_vbus(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    yaml_config = yaml_config.replace(
        "EXTERNAL_COMPONENT_PATH",
        str(Path(__file__).parent / "fixtures" / "external_components"),
    )
    received: list[str] = []
    writes: list[str] = []
    complete = asyncio.Event()

    def on_line(line: str) -> None:
        if "[vbus.received:" in line:
            received.append(
                re.sub(r"\x1b\[[0-9;]*m", "", line).rsplit(": ", 1)[-1].strip()
            )
            if len(received) >= 2:
                complete.set()
        if "uart_mock" in line and "TX " in line:
            writes.append(line)

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        start = find_entity(entities, "start_scenario", ButtonInfo)
        assert start is not None
        client.button_command(start.key)
        await asyncio.wait_for(complete.wait(), timeout=5.0)
        await asyncio.sleep(0.1)
        assert sorted(received) == [
            "0600 1234 0000 0000 00000000",
            "0900 2211 0010 9234 89abcdef",
        ]
        assert not writes
