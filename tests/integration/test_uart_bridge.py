"""Integration tests for the bridge uart platform.

A modbus client hub and a modbus server hub talk through the bridge:
client hub - client_line ~ end_a - bridge - end_b ~ server_line - server hub.
Each pair joined by ~ is a wireless null-modem link; what one end writes, the other receives.

test_uart_bridge_virtual:
  Every end is a VirtualUARTComponent, so the bridge is wired directly and each frame crosses in one
  write: an 8-byte read request, the 209-byte write request and the 255-byte reply to a 125-register read.

test_uart_bridge_polled:
  Every end is a uart_mock, so the bridge polls both ends and collects each frame until the end has been
  quiet for a frame gap. uart_mock reports a baud rate of 115200, so it is timed as a line: 3.5 characters
  are less than 1.75 ms there, so the gap is 1750 us. uart_mock cannot report its free room, so every
  frame leaves in one write.

test_uart_bridge_copy:
  meter ~ end_a - bridge - end_b. end_a is virtual but passes on what meter writes, piece by piece, like
  a UART that copies another one, so the bridge reads it like a line at 115200 baud; end_b is a uart_mock.
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
_END_KIND = re.compile(r"\b([AB]): (virtual|polled), frame gap (\d+) us")


class _BridgeLog:
    """Collects the bridge traffic seen in the device log."""

    def __init__(self) -> None:
        loop = asyncio.get_running_loop()
        self.small = loop.create_future()
        self.large = loop.create_future()
        self.write_reply = loop.create_future()
        self.tx: dict[str, list[int]] = {"end_a": [], "end_b": []}
        self.end_kinds: dict[str, tuple[str, int]] = {}

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
            self.end_kinds[match.group(1)] = (match.group(2), int(match.group(3)))

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

    assert log.end_kinds == {"A": ("virtual", 0), "B": ("virtual", 0)}
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

    assert log.end_kinds == {"A": ("polled", 1750), "B": ("polled", 1750)}
    # Each frame leaves whole: read requests, the 209-byte write request, the 255-byte reply.
    assert 8 in log.tx["end_b"]
    assert 209 in log.tx["end_b"]
    assert 255 in log.tx["end_a"]


@pytest.mark.asyncio
async def test_uart_bridge_copy(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """An end that copies a UART: a frame in pieces leaves whole, a telegram loses no byte."""
    log = _BridgeLog()
    done = asyncio.get_running_loop().create_future()

    def on_line(line: str) -> None:
        log.on_line(line)
        if "METER DONE" in line and not done.done():
            done.set_result(None)

    async with (
        run_compiled(yaml_config, line_callback=on_line),
        api_client_connected(),
    ):
        await asyncio.wait_for(done, timeout=15.0)

    assert log.end_kinds == {"A": ("polled", 1750), "B": ("polled", 1750)}
    # 30: the three pieces of 10 bytes leave as one frame, in one write.
    # Each later step writes four pieces of 64 at once; they fill end_a's 256-byte buffer and
    # one bridge block exactly. end_b cannot report its free room, so a full block goes out at
    # the line rate: at most 16 ms of its 115200 line per write, 185 bytes.
    # 30 + four times 256: no byte of the telegram is lost.
    assert log.tx["end_b"][0] == 30
    assert max(log.tx["end_b"]) == 185
    assert sum(log.tx["end_b"]) == 30 + 4 * 256
