"""Integration tests for the bridge uart platform.

A modbus client hub and a modbus server hub talk through the bridge:
client hub - client_line ~ end_a - bridge - end_b ~ server_line - server hub.
Each pair joined by ~ is a wireless null-modem link; what one end writes, the other receives.

test_uart_bridge_virtual:
  Every end is a VirtualUARTComponent, so the bridge is wired directly and each frame crosses in one
  write: an 8-byte read request, the 209-byte write request and the 255-byte reply to a 125-register read.

test_uart_bridge_polled:
  Every end is a uart_mock, so the bridge polls both ends and collects each frame until the end has been
  quiet for 50 ms (uart_mock is not a hardware line). uart_mock cannot report its free room, so every
  frame leaves in one write.
"""

from __future__ import annotations

import asyncio
import re

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

_TX = re.compile(r"(end_[ab]) TX (\d+)")
_SMALL = re.compile(r"BRIDGE_SMALL (\d+)")
_LARGE = re.compile(r"BRIDGE_LARGE (\d+) ([0-9a-f]+)")
# The server has no register at 0x0100, so it answers the 100-register write with exception 2.
_WRITE_REPLY = re.compile(r"Error function code: 0x90 exception: 2")
# dump_config continuation lines carry no tag of their own.
_END_KIND = re.compile(r"\b([AB]): (virtual|polled), frame gap")


class _BridgeLog:
    """Collects the bridge traffic seen in the device log."""

    def __init__(self) -> None:
        loop = asyncio.get_running_loop()
        self.small = loop.create_future()
        self.large = loop.create_future()
        self.write_reply = loop.create_future()
        self.tx: dict[str, list[int]] = {"end_a": [], "end_b": []}
        self.end_kinds: dict[str, str] = {}

    def on_line(self, line: str) -> None:
        if (match := _TX.search(line)) is not None:
            self.tx[match.group(1)].append(int(match.group(2)))
        if (match := _SMALL.search(line)) is not None and not self.small.done():
            self.small.set_result(int(match.group(1)))
        if (match := _LARGE.search(line)) is not None and not self.large.done():
            self.large.set_result((int(match.group(1)), match.group(2)))
        if _WRITE_REPLY.search(line) and not self.write_reply.done():
            self.write_reply.set_result(True)
        if (match := _END_KIND.search(line)) is not None:
            self.end_kinds[match.group(1)] = match.group(2)

    async def wait_for_traffic(self) -> None:
        await asyncio.wait_for(
            asyncio.gather(self.small, self.large, self.write_reply), timeout=15.0
        )
        assert self.small.result() == 259
        assert self.large.result() == (500, "12341234")


@pytest.mark.asyncio
async def test_uart_bridge_virtual(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Virtual ends: every frame crosses the bridge in one write."""
    log = _BridgeLog()
    async with (
        run_compiled(yaml_config, line_callback=log.on_line),
        api_client_connected(),
    ):
        await log.wait_for_traffic()

    assert log.end_kinds == {"A": "virtual", "B": "virtual"}
    # Read requests (8 bytes) and the 100-register write request (209 bytes) reach end_b whole.
    assert 8 in log.tx["end_b"]
    assert 209 in log.tx["end_b"]
    # The 125-register reply (255 bytes) reaches end_a whole.
    assert 255 in log.tx["end_a"]


@pytest.mark.asyncio
async def test_uart_bridge_polled(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Polled ends: frames are collected until the line is quiet and arrive intact."""
    log = _BridgeLog()
    async with (
        run_compiled(yaml_config, line_callback=log.on_line),
        api_client_connected(),
    ):
        await log.wait_for_traffic()

    assert log.end_kinds == {"A": "polled", "B": "polled"}
    # Each frame leaves whole: read requests, the 209-byte write request, the 255-byte reply.
    assert 8 in log.tx["end_b"]
    assert 209 in log.tx["end_b"]
    assert 255 in log.tx["end_a"]
