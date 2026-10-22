"""Integration test for the LD2410 selects with mock UART.

The module answers the distance resolution query with 0x00, so the select starts at 0.75m, and the
baud rate select starts at the bus speed. Choosing an option must send the device code of that
option: 0.2m is 0x01 and 19200 is the second baud rate.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import SelectInfo
import pytest

from .state_utils import InitialStateHelper, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

# Length and command word of each frame as the uart mock logs it; the payload is logged on its own TX line
SET_RESOLUTION = "04:00:AA:00"
SET_BAUD_RATE = "04:00:A1:00"


class _TxLines:
    """Record the TX lines and hand out the payload line that follows an awaited command line."""

    def __init__(self) -> None:
        self.lines: list[str] = []
        # Command text, whether its line was seen, and the future for the line after it
        self._awaited: list[tuple[str, bool, asyncio.Future[str]]] = []

    def on_line(self, line: str) -> None:
        if "uart_mock" not in line or "TX " not in line:
            return
        self.lines.append(line)
        for i, (text, seen, future) in enumerate(self._awaited):
            if seen:
                if not future.done():
                    future.set_result(line)
            elif text in line:
                self._awaited[i] = (text, True, future)

    def next_payload_after(self, text: str) -> asyncio.Future[str]:
        """Resolve with the TX line logged right after the next line holding ``text``."""
        future: asyncio.Future[str] = asyncio.get_running_loop().create_future()
        self._awaited.append((text, False, future))
        return future


@pytest.mark.asyncio
async def test_uart_mock_ld2410_selects(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that the selects show the module's values and send the device code of the chosen option."""
    events = _TxLines()

    async with (
        run_compiled(yaml_config, line_callback=events.on_line),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(lambda s: None))
        await initial_state_helper.wait_for_initial_states()
        resolution = require_entity(entities, "distance_resolution", SelectInfo)
        baud_rate = require_entity(entities, "baud_rate", SelectInfo)
        assert initial_state_helper.initial_states[resolution.key].state == "0.75m"
        # The mock bus runs at 256000, which the select reports at boot
        assert initial_state_helper.initial_states[baud_rate.key].state == "256000"

        # Each payload line follows its length and command line directly
        resolution_payload = events.next_payload_after(SET_RESOLUTION)
        client.select_command(resolution.key, "0.2m")
        await wait_or_fail(
            resolution_payload,
            5.0,
            "Timeout waiting for the distance resolution command",
            logged=events.lines,
        )
        assert "TX 2 bytes: 01:00" in resolution_payload.result()

        baud_rate_payload = events.next_payload_after(SET_BAUD_RATE)
        client.select_command(baud_rate.key, "19200")
        await wait_or_fail(
            baud_rate_payload,
            5.0,
            "Timeout waiting for the baud rate command",
            logged=events.lines,
        )
        assert "TX 2 bytes: 02:00" in baud_rate_payload.result()
