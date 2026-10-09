"""Integration test for the rfc2217_uart server on host.

The UART is backed by a pty; pySerial's rfc2217:// client connects as the
RFC 2217 client.
"""

from __future__ import annotations

import asyncio
import os
import pathlib

import pytest
import serial

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


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
    unused_tcp_port_factory,
) -> None:
    server_port = unused_tcp_port_factory()
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

    lines = LineWaiter()
    try:
        pty_link.symlink_to(os.ttyname(device_fd))
        yaml_config = yaml_config.replace("port: 18128", f"port: {server_port}")
        yaml_config = yaml_config.replace("PTY_PATH", str(pty_link))
        loop.add_reader(controller_fd, on_controller_readable)
        async with (
            run_compiled(yaml_config, line_callback=lines.callback),
            api_client_connected() as client,
        ):
            assert (await client.device_info()).name == "rfc2217-uart-test"
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
            await lines.wait_for("Connection lost")

            # ?ign_set_control skips the answers to SET-CONTROL.
            port = await asyncio.to_thread(_open, server_port, "ign_set_control")
            try:
                await asyncio.to_thread(port.write, b"ok")
                assert await read_uart(2) == b"ok"
            finally:
                await asyncio.to_thread(port.close)
    finally:
        loop.remove_reader(controller_fd)
        os.close(controller_fd)
        os.close(device_fd)
        pty_link.unlink(missing_ok=True)
