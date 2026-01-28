"""Integration test for MQTT in Host mode."""

from __future__ import annotations

import asyncio
import contextlib

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_mqtt(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test Host mode starts and MQTT connects to a local broker stub."""
    loop = asyncio.get_running_loop()
    mqtt_connected: asyncio.Future[None] = loop.create_future()

    async def handle_client(
        reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        """Minimal MQTT broker stub: accept CONNECT, reply CONNACK."""
        try:
            # Read some bytes (enough to contain CONNECT packet header/remaining length)
            await reader.read(1024)
            # CONNACK: fixed header 0x20, remaining length 0x02, flags 0, return code 0 (accepted)
            writer.write(b"\x20\x02\x00\x00")
            await writer.drain()
            # Keep connection open until client disconnects
            await reader.read()
        finally:
            writer.close()
            with contextlib.suppress(Exception):
                await writer.wait_closed()

    server = await asyncio.start_server(handle_client, host="127.0.0.1", port=0)
    try:
        port = server.sockets[0].getsockname()[1]
        yaml_config = yaml_config.replace("__MQTT_PORT__", str(port))

        def on_line(line: str) -> None:
            # Expect: [I][mqtt:xxxx] Connected
            if "][mqtt:" in line and " Connected" in line and not mqtt_connected.done():
                mqtt_connected.set_result(None)

        async with (
            run_compiled(yaml_config, line_callback=on_line),
            api_client_connected() as client,
        ):
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "host-mqtt-test"

            # Wait for MQTT to connect to our stub broker.
            await asyncio.wait_for(mqtt_connected, timeout=10.0)
    finally:
        server.close()
        await server.wait_closed()
