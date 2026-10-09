"""Integration test for rfc2217_uart on host.

The server's UART is backed by a pty and pySerial's rfc2217:// client connects
to it. The client role connects to a scripted access server in the test, and a
lambda echoes what it reads.
"""

from __future__ import annotations

import asyncio
from collections.abc import Callable
import os
import pathlib

import pytest
import serial

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction

OFFERS = b"\xff\xfb\x00\xff\xfd\x00\xff\xfb\x2c"
DO_COM_PORT = b"\xff\xfd\x2c"
# 19200 baud, 8 data bits, EVEN parity, 1 stop bit.
SETTINGS = (
    b"\xff\xfa\x2c\x01\x00\x00\x4b\x00\xff\xf0"
    b"\xff\xfa\x2c\x02\x08\xff\xf0"
    b"\xff\xfa\x2c\x03\x03\xff\xf0"
    b"\xff\xfa\x2c\x04\x01\xff\xf0"
)
SERVER_SUSPEND = b"\xff\xfa\x2c\x6c\xff\xf0"
SERVER_RESUME = b"\xff\xfa\x2c\x6d\xff\xf0"


def _open(port: int, options: str = "", **settings: object) -> serial.Serial:
    url = f"rfc2217://127.0.0.1:{port}"
    if options:
        url += f"?{options}"
    client = serial.serial_for_url(url, baudrate=115200, timeout=2, do_not_open=True)
    for key, value in settings.items():
        setattr(client, key, value)
    client.open()
    return client


@pytest.mark.asyncio
async def test_rfc2217_uart(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    unused_tcp_port_factory: Callable[[], int],
) -> None:
    server_port = unused_tcp_port_factory()
    client_port = unused_tcp_port_factory()
    controller_fd, device_fd = os.openpty()
    os.set_blocking(controller_fd, False)
    # uart's validate_port wants a two segment device path.
    pty_link = pathlib.Path(f"/tmp/rfc2217-uart-pty-{os.getpid()}")
    loop = asyncio.get_running_loop()
    uart_rx = bytearray()
    uart_rx_event = asyncio.Event()

    def on_controller_readable() -> None:
        try:
            chunk = os.read(controller_fd, 256)
        except BlockingIOError:
            return
        if chunk:
            uart_rx.extend(chunk)
            uart_rx_event.set()

    async def read_uart(count: int, timeout: float = 10.0) -> bytes:
        while len(uart_rx) < count:
            uart_rx_event.clear()
            await asyncio.wait_for(uart_rx_event.wait(), timeout)
        data = bytes(uart_rx[:count])
        del uart_rx[:count]
        return data

    peers: asyncio.Queue[tuple[asyncio.StreamReader, asyncio.StreamWriter]] = (
        asyncio.Queue()
    )
    writers: list[asyncio.StreamWriter] = []

    async def on_peer(
        reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        writers.append(writer)
        await peers.put((reader, writer))

    lines = LineWaiter()
    access_server: asyncio.Server | None = None

    async def wait_for_drops(count: int) -> None:
        # wait_for() also matches an earlier drop.
        async with asyncio.timeout(10):
            while sum("Connection lost" in line for line in lines.lines) < count:
                await asyncio.sleep(0.05)

    try:
        access_server = await asyncio.start_server(on_peer, "127.0.0.1", client_port)
        pty_link.symlink_to(os.ttyname(device_fd))
        yaml_config = yaml_config.replace("port: 18128", f"port: {server_port}")
        yaml_config = yaml_config.replace("port: 18129", f"port: {client_port}")
        yaml_config = yaml_config.replace("PTY_PATH", str(pty_link))
        loop.add_reader(controller_fd, on_controller_readable)
        async with (
            run_compiled(yaml_config, line_callback=lines.callback),
            api_client_connected() as client,
        ):
            assert (await client.device_info()).name == "rfc2217-uart-test"

            # Client role: offers, then the settings once COM-PORT is accepted.
            reader, writer = await asyncio.wait_for(peers.get(), 15)
            assert await asyncio.wait_for(reader.readexactly(9), 10) == OFFERS
            writer.write(DO_COM_PORT)
            await writer.drain()
            assert await asyncio.wait_for(reader.readexactly(31), 10) == SETTINGS
            # A doubled IAC is one 0xFF for the reader and doubled again on the way back.
            writer.write(b"\x11\x13\xff\xff\x41")
            await writer.drain()
            echo = await asyncio.wait_for(reader.readexactly(5), 10)
            assert echo == b"\x11\x13\xff\xff\x41"
            # SUSPEND holds the echo, not one byte of it, until RESUME.
            writer.write(SERVER_SUSPEND + b"held")
            await writer.drain()
            with pytest.raises(TimeoutError):
                await asyncio.wait_for(reader.read(1), 0.5)
            writer.write(SERVER_RESUME)
            await writer.drain()
            assert await asyncio.wait_for(reader.readexactly(4), 10) == b"held"
            await lines.wait_for("Listening on")

            # pySerial asks for DTR and RTS on; the UART has neither line.
            with pytest.raises(ValueError, match="'control'"):
                await asyncio.to_thread(_open, server_port, "timeout=1")
            # Asking for them off matches the answer.
            port = await asyncio.to_thread(_open, server_port, dtr=False, rts=False)
            try:
                await asyncio.to_thread(port.write, b"\x11\x13\xffABC")
                assert await read_uart(6) == b"\x11\x13\xffABC"
                os.write(controller_fd, b"\xff\x13xyz\x11")
                assert await asyncio.to_thread(port.read, 6) == b"\xff\x13xyz\x11"
                await asyncio.to_thread(port.reset_input_buffer)
                await asyncio.to_thread(port.reset_output_buffer)
                # The host UART cannot change its line, so the answer keeps 115200.
                with pytest.raises(ValueError, match="'baudrate'"):
                    await asyncio.to_thread(setattr, port, "baudrate", 9600)
            finally:
                await asyncio.to_thread(port.close)
            await wait_for_drops(2)

            # Bytes written right before the close still reach the UART.
            port = await asyncio.to_thread(_open, server_port, dtr=False, rts=False)
            data = bytes(i % 250 for i in range(1000))
            await asyncio.to_thread(port.write, data)
            await asyncio.to_thread(port.close)
            assert await read_uart(len(data)) == data
            await wait_for_drops(3)

            # ?ign_set_control skips the answers to SET-CONTROL.
            port = await asyncio.to_thread(_open, server_port, "ign_set_control")
            try:
                await asyncio.to_thread(port.write, b"ok")
                assert await read_uart(2) == b"ok"
            finally:
                await asyncio.to_thread(port.close)
    finally:
        for peer_writer in writers:
            peer_writer.close()
        if access_server is not None:
            access_server.close()
            await access_server.wait_closed()
        loop.remove_reader(controller_fd)
        os.close(controller_fd)
        os.close(device_fd)
        pty_link.unlink(missing_ok=True)
