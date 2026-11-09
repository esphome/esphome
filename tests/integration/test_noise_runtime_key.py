"""Integration test for a link whose key is set at runtime, on host: no session
without a key, the api key refused, and the saved key used again after a restart."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator, Callable
import contextlib
import socket

import pytest

from .conftest import run_binary_and_wait_for_port, wait_and_connect_api_client
from .log_utils import LineWaiter
from .types import CompileFunction, ConfigWriter

API_KEY = "cnVudGltZSBrZXkgdGVzdDogdGhlIGFwaSBrZXkhISE="
LINK_KEY = "cnVudGltZSBrZXkgdGVzdDogdGhlIGxpbmsga2V5ISE="


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
    link_port = unused_tcp_port_factory()

    @contextlib.asynccontextmanager
    async def run(lines: LineWaiter) -> AsyncIterator:
        async with (
            run_binary_and_wait_for_port(
                binary_path, "127.0.0.1", port, line_callback=lines.callback
            ),
            wait_and_connect_api_client(port=port, noise_psk=API_KEY) as client,
        ):
            yield client

    yaml_config = yaml_config.replace("port: 18136", f"port: {link_port}")
    binary_path = await compile_esphome(await write_yaml_config(yaml_config))
    port_socket.close()

    lines = LineWaiter()
    async with run(lines) as client:
        # Both ends start without a key: the server does not listen
        await lines.wait_count("the link stays closed until one is set", 2, timeout=15)
        assert await _refused(link_port)

        _, services = await client.list_entities_services()
        set_key = next(s for s in services if s.name == "set_link_key")

        # The api key never becomes the link key
        await client.execute_service(set_key, {"key": API_KEY})
        await lines.wait_count("Rejecting the api or ota encryption key", 2, timeout=15)
        assert await _refused(link_port)

        # The link key on both ends brings the session up, and 512 bytes cross
        await client.execute_service(set_key, {"key": LINK_KEY})
        await lines.wait_count("Encryption key saved", 2, timeout=15)
        await lines.wait_for("Echo complete")
        assert lines.count("Echo mismatch") == 0

    # After a restart both ends use the saved key
    lines = LineWaiter()
    async with run(lines):
        await lines.wait_for("Echo complete")
        assert lines.count("the link stays closed") == 0
        assert lines.count("Echo mismatch") == 0
