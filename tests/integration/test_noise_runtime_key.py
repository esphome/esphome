"""Integration test for a link whose key is set at runtime, on host: no session
without a key, the api key refused, and the saved key used again after a restart."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator, Callable
import contextlib
import os
import pathlib
import socket

import pytest

from .conftest import run_binary_and_wait_for_port, wait_and_connect_api_client
from .log_utils import LineWaiter
from .types import CompileFunction, ConfigWriter

API_KEY = "cnVudGltZSBrZXkgdGVzdDogdGhlIGFwaSBrZXkhISE="
LINK_KEY = "cnVudGltZSBrZXkgdGVzdDogdGhlIGxpbmsga2V5ISE="


def _count(lines: list[str], text: str) -> int:
    return sum(text in line for line in lines)


async def _wait_count(lines: list[str], text: str, count: int) -> None:
    async with asyncio.timeout(15):
        while _count(lines, text) < count:
            await asyncio.sleep(0.05)


async def _refused(port: int) -> bool:
    try:
        _, writer = await asyncio.open_connection("127.0.0.1", port)
    except ConnectionRefusedError:
        return True
    writer.close()
    return False


@pytest.mark.asyncio
async def test_noise_runtime_key(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
    unused_tcp_port_factory: Callable[[], int],
) -> None:
    port, port_socket = reserved_tcp_port
    bridge_port = unused_tcp_port_factory()
    controller_fd, device_fd = os.openpty()
    os.set_blocking(controller_fd, False)
    # uart's validate_port wants a two segment device path.
    pty_link = pathlib.Path(f"/tmp/noise-runtime-key-pty-{os.getpid()}")
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

    async def echo(data: bytes) -> bytes:
        assert os.write(controller_fd, data) == len(data)
        while len(uart_rx) < len(data):
            uart_rx_event.clear()
            await asyncio.wait_for(uart_rx_event.wait(), 10)
        out = bytes(uart_rx[: len(data)])
        del uart_rx[: len(data)]
        return out

    @contextlib.asynccontextmanager
    async def run(lines: LineWaiter) -> AsyncIterator:
        async with (
            run_binary_and_wait_for_port(
                binary_path, "127.0.0.1", port, line_callback=lines.callback
            ),
            wait_and_connect_api_client(port=port, noise_psk=API_KEY) as client,
        ):
            yield client

    try:
        pty_link.symlink_to(os.ttyname(device_fd))
        yaml_config = yaml_config.replace("port: 18136", f"port: {bridge_port}")
        yaml_config = yaml_config.replace("PTY_PATH", str(pty_link))
        binary_path = await compile_esphome(await write_yaml_config(yaml_config))
        port_socket.close()
        loop.add_reader(controller_fd, on_controller_readable)
        data = bytes(range(256)) * 2

        lines = LineWaiter()
        async with run(lines) as client:
            # Both ends start without a key: the server does not listen
            await _wait_count(lines.lines, "the link stays closed until one is set", 2)
            assert await _refused(bridge_port)

            _, services = await client.list_entities_services()
            set_key = next(s for s in services if s.name == "set_link_key")

            # The api key never becomes the link key
            await client.execute_service(set_key, {"key": API_KEY})
            await _wait_count(lines.lines, "Rejecting the api or ota encryption key", 2)
            assert await _refused(bridge_port)

            # The link key on both ends brings the session up
            await client.execute_service(set_key, {"key": LINK_KEY})
            await _wait_count(lines.lines, "Encryption key saved", 2)
            await _wait_count(lines.lines, "Session encrypted", 2)
            assert await echo(data) == data

        # After a restart both ends use the saved key
        lines = LineWaiter()
        async with run(lines):
            await _wait_count(lines.lines, "Session encrypted", 2)
            assert await echo(data) == data
            assert _count(lines.lines, "the link stays closed") == 0
    finally:
        loop.remove_reader(controller_fd)
        os.close(controller_fd)
        os.close(device_fd)
        pty_link.unlink(missing_ok=True)
