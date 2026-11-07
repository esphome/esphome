"""uart_split gives each output its own copy of what its UART receives.

The fixture's uart_mock receives one Modbus RTU request. Two outputs read it: bus, which
may write, and tap, which is receive-only. Each output's debugger logs the bytes it read.
"""

from __future__ import annotations

import re

import pytest

from .log_utils import LineWaiter
from .types import RunCompiledFunction

# Each output's debugger logs "<output> <<< <the bytes it read, in hex>"
RECEIVED = re.compile(r"\b(bus|tap) <<< ([0-9A-F:]+)")

# The injected request: device 1, function 03 (read holding registers),
# register 0x0000, 1 register, CRC 0x0A84 (low byte first)
REQUEST = "01:03:00:00:00:01:84:0A"


@pytest.mark.asyncio
async def test_uart_mock_uart_split(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
) -> None:
    """Both outputs read exactly the received bytes, in order, and nothing else."""
    waiter = LineWaiter()
    async with run_compiled(yaml_config, line_callback=waiter.callback):
        await waiter.wait_count("bus <<<", 1, msg="bus read nothing")
        await waiter.wait_count("tap <<<", 1, msg="tap read nothing")

    received: dict[str, list[str]] = {"bus": [], "tap": []}
    for line in waiter.lines:
        if (match := RECEIVED.search(line)) is not None:
            received[match.group(1)].append(match.group(2))
    # One line each: the eight bytes arrive together, and the debugger logs what it
    # collected once 100 ms pass without a new byte
    assert received == {"bus": [REQUEST], "tap": [REQUEST]}
