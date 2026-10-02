"""Integration test for the tcp_uart bridge on host.

Pytest runs a real TCP server; a lambda echoes UART bytes back to the socket.
Covers the UART read and write paths, a successful flush, the offline drop
warning and the automatic reconnect.
"""

from __future__ import annotations

import asyncio
import contextlib

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

GREETING = b"hi"
RECONNECT_GREETING = b"yo"


@pytest.mark.asyncio
async def test_tcp_uart_bridge(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory,
) -> None:
    server_port = unused_tcp_port_factory()
    yaml_config = yaml_config.replace("port: 18125", f"port: {server_port}")

    echo_ok = asyncio.Event()
    service_byte_ok = asyncio.Event()
    flush_success = asyncio.Event()
    drop_warning = asyncio.Event()
    connection_lost = asyncio.Event()
    reconnect_echo_ok = asyncio.Event()
    sessions = 0

    def on_log_line(line: str) -> None:
        if "Flush result 0" in line:
            flush_success.set()
        elif "Not connected, dropped" in line:
            drop_warning.set()
        elif "Connection lost" in line:
            connection_lost.set()

    async def handle(
        reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        nonlocal sessions
        sessions += 1
        if sessions == 1:
            writer.write(GREETING)
            await writer.drain()
            with contextlib.suppress(TimeoutError, asyncio.IncompleteReadError):
                if (
                    await asyncio.wait_for(reader.readexactly(len(GREETING)), 10)
                    == GREETING
                ):
                    echo_ok.set()
                # The send_byte service writes one 0x58 through the UART.
                if await asyncio.wait_for(reader.readexactly(1), 10) == b"X":
                    service_byte_ok.set()
            writer.close()
            return
        writer.write(RECONNECT_GREETING)
        await writer.drain()
        with contextlib.suppress(TimeoutError, asyncio.IncompleteReadError):
            if (
                await asyncio.wait_for(reader.readexactly(len(RECONNECT_GREETING)), 10)
                == RECONNECT_GREETING
            ):
                reconnect_echo_ok.set()

    server = await asyncio.start_server(handle, "127.0.0.1", server_port)
    try:
        async with (
            run_compiled(yaml_config, line_callback=on_log_line),
            api_client_connected() as client,
        ):
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "tcp-uart-bridge-test"
            entities, services = await client.list_entities_services()
            send_byte = next(s for s in services if s.name == "send_byte")

            try:
                await asyncio.wait_for(echo_ok.wait(), timeout=15.0)
            except TimeoutError:
                pytest.fail("UART echo through the bridge never arrived")

            await client.execute_service(send_byte, {})
            try:
                await asyncio.wait_for(service_byte_ok.wait(), timeout=10.0)
                await asyncio.wait_for(flush_success.wait(), timeout=10.0)
            except TimeoutError:
                pytest.fail("Service byte or successful flush not observed")

            # The server closed session one; a write while down must warn.
            try:
                await asyncio.wait_for(connection_lost.wait(), timeout=10.0)
            except TimeoutError:
                pytest.fail("Bridge never noticed the drop")
            await client.execute_service(send_byte, {})
            try:
                await asyncio.wait_for(drop_warning.wait(), timeout=10.0)
            except TimeoutError:
                pytest.fail("Offline write did not log the drop warning")

            try:
                await asyncio.wait_for(reconnect_echo_ok.wait(), timeout=15.0)
            except TimeoutError:
                pytest.fail("Bridge did not reconnect and echo again")
    finally:
        server.close()
        await server.wait_closed()
