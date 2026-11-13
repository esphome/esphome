"""Integration tests for modbus_tcp_uart on host, one per role.

Pytest is the other end of the Modbus TCP link and writes the frames by hand:
in the client test it serves a register the firmware polls, in the server test
it reads a register the firmware serves.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import SensorInfo, SensorState
import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name, require_entity, wait_for_state
from .tcp_utils import tcp_server
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

# The firmware's request for the Grid power sensor: read one holding register.
READ_GRID_POWER = (
    b"\x00\x01"  # transaction id 1: the firmware's first request
    b"\x00\x00"  # protocol id 0: Modbus
    b"\x00\x06"  # length: the 6 bytes that follow
    b"\x01"  # unit id 1: the modbus_controller's address
    b"\x03"  # function code 3: read holding registers
    b"\x00\x00"  # first register: 0x0000
    b"\x00\x01"  # one register
)

# Pytest's response as the meter: register 0x0000 holds 1500 (0x05DC).
GRID_POWER_RESPONSE = (
    b"\x00\x01"  # transaction id 1: the same as the request
    b"\x00\x00"  # protocol id 0: Modbus
    b"\x00\x05"  # length: the 5 bytes that follow
    b"\x01"  # unit id 1
    b"\x03"  # function code 3
    b"\x02"  # byte count: one register is 2 bytes
    b"\x05\xdc"  # 1500
)
GRID_POWER = 1500.0

# Pytest's request as the client: read the two registers of the float at 0x0000.
READ_SIGNAL = (
    b"\x12\x34"  # transaction id 0x1234: any id the client picks
    b"\x00\x00"  # protocol id 0: Modbus
    b"\x00\x06"  # length: the 6 bytes that follow
    b"\x01"  # unit id 1: the modbus_server's address
    b"\x03"  # function code 3: read holding registers
    b"\x00\x00"  # first register: 0x0000
    b"\x00\x02"  # two registers: one 32-bit float
)

# The firmware's response: -60.0 as a 32-bit float is 0xC2700000, high word first.
SIGNAL_RESPONSE = (
    b"\x12\x34"  # transaction id 0x1234: the same as the request
    b"\x00\x00"  # protocol id 0: Modbus
    b"\x00\x07"  # length: the 7 bytes that follow
    b"\x01"  # unit id 1
    b"\x03"  # function code 3
    b"\x04"  # byte count: two registers are 4 bytes
    b"\xc2\x70\x00\x00"  # -60.0
)
SIGNAL = -60.0


@pytest.mark.asyncio
async def test_modbus_tcp_uart_client(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory,
) -> None:
    """The firmware polls a holding register from pytest's Modbus TCP server.

    Checks the request on the wire and that the sensor shows the register value.
    """
    meter_port = unused_tcp_port_factory()
    yaml_config = yaml_config.replace("port: 18128", f"port: {meter_port}")

    received: asyncio.Queue[bytes] = asyncio.Queue()

    async def meter(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
        received.put_nowait(await reader.readexactly(len(READ_GRID_POWER)))
        writer.write(GRID_POWER_RESPONSE)
        await writer.drain()

    async with (
        tcp_server(meter, meter_port),
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "modbus-tcp-uart-client-test")
        entities, _ = await client.list_entities_services()
        grid_power = require_entity(entities, "grid_power", SensorInfo)

        request = await wait_or_fail(
            received.get(), 15.0, "The firmware sent no Modbus TCP request"
        )
        assert request == READ_GRID_POWER

        await wait_for_state(
            client,
            lambda state: (
                isinstance(state, SensorState)
                and state.key == grid_power.key
                and state.state == GRID_POWER
            ),
            timeout=10.0,
            msg="Grid power never showed the register value",
        )


@pytest.mark.asyncio
async def test_modbus_tcp_uart_server(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory,
) -> None:
    """Pytest reads a register the firmware serves, as a Modbus TCP client.

    Checks the whole response: the request's transaction id, the unit id, the
    length and the PDU.
    """
    server_port = unused_tcp_port_factory()
    yaml_config = yaml_config.replace("port: 18129", f"port: {server_port}")

    lines = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=lines.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "modbus-tcp-uart-server-test")
        entities, _ = await client.list_entities_services()
        signal_sensor = require_entity(entities, "signal", SensorInfo)
        # The register serves the sensor's state, so wait until the sensor has one.
        await wait_for_state(
            client,
            lambda state: (
                isinstance(state, SensorState)
                and state.key == signal_sensor.key
                and state.state == SIGNAL
            ),
            msg="Signal never showed its value",
        )
        await lines.wait_for(f"Listening on {server_port}")

        reader, writer = await asyncio.open_connection("127.0.0.1", server_port)
        try:
            await lines.wait_for("Client connected from 127.0.0.1")
            writer.write(READ_SIGNAL)
            await writer.drain()
            response = await wait_or_fail(
                reader.readexactly(len(SIGNAL_RESPONSE)),
                10.0,
                "The firmware sent no Modbus TCP response",
            )
        finally:
            writer.close()
        assert response == SIGNAL_RESPONSE
