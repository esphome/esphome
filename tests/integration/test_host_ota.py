"""End-to-end OTA tests on the host platform.

Exercises the native OTA protocol against a real host binary, then asserts
pid is preserved across the post-OTA execv. A second OTA on the post-exec
instance covers the FD_CLOEXEC path.
"""

from __future__ import annotations

import asyncio
from collections.abc import Generator
from contextlib import contextmanager
import socket

import pytest

from esphome import espota2

from .conftest import run_binary, wait_and_connect_api_client
from .const import LOCALHOST, PORT_POLL_INTERVAL, PORT_WAIT_TIMEOUT
from .types import CompileFunction, ConfigWriter

DEVICE_NAME = "host-ota-test"


@contextmanager
def _reserve_port() -> Generator[tuple[int, socket.socket]]:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("", 0))
    try:
        yield s.getsockname()[1], s
    finally:
        s.close()


async def _wait_for_port(host: str, port: int, timeout: float) -> None:
    """Poll until a TCP port accepts connections, or raise TimeoutError."""
    loop = asyncio.get_running_loop()
    deadline = loop.time() + timeout
    while loop.time() < deadline:
        try:
            _, writer = await asyncio.open_connection(host, port)
        except (ConnectionRefusedError, OSError):
            await asyncio.sleep(PORT_POLL_INTERVAL)
            continue
        writer.close()
        await writer.wait_closed()
        return
    raise TimeoutError(f"Port {port} on {host} did not open within {timeout}s")


@pytest.mark.asyncio
async def test_host_ota_self_update(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> None:
    """Self-OTA: upload the running binary back to itself, expect re-exec."""
    api_port, api_socket = reserved_tcp_port
    with _reserve_port() as (ota_port, ota_socket):
        yaml_config = yaml_config.replace("__OTA_PORT__", str(ota_port))
        config_path = await write_yaml_config(yaml_config)
        binary_path = await compile_esphome(config_path)
        api_socket.close()
        ota_socket.close()

        loop = asyncio.get_running_loop()
        ota_staged = loop.create_future()
        rebooted = loop.create_future()

        def on_log(line: str) -> None:
            if not ota_staged.done() and "OTA staged at" in line:
                ota_staged.set_result(True)
            if not rebooted.done() and "Rebooting safely" in line:
                rebooted.set_result(True)

        async with run_binary(binary_path, line_callback=on_log) as (proc, _lines):
            await _wait_for_port(LOCALHOST, api_port, PORT_WAIT_TIMEOUT)
            pid_before = proc.pid
            async with wait_and_connect_api_client(port=api_port) as client:
                info_before = await client.device_info()
                assert info_before.name == DEVICE_NAME

            # espota2 is blocking; run in executor.
            rc, _ = await loop.run_in_executor(
                None, espota2.run_ota, LOCALHOST, ota_port, None, binary_path
            )
            assert rc == 0, "espota2 reported failure"

            await asyncio.wait_for(ota_staged, timeout=10.0)
            await asyncio.wait_for(rebooted, timeout=10.0)
            await _wait_for_port(LOCALHOST, api_port, PORT_WAIT_TIMEOUT)

            # execv preserves pid; mismatch means external respawn.
            assert proc.returncode is None, "process exited instead of execing"
            assert proc.pid == pid_before

            async with wait_and_connect_api_client(port=api_port) as client:
                info_after = await client.device_info()
                assert info_after.name == DEVICE_NAME
                assert info_after.name == info_before.name

            # Second OTA: catches FD_CLOEXEC regressions (EADDRINUSE on rebind).
            rc, _ = await loop.run_in_executor(
                None, espota2.run_ota, LOCALHOST, ota_port, None, binary_path
            )
            assert rc == 0, "second OTA failed -- listener leaked across execv"
            await _wait_for_port(LOCALHOST, api_port, PORT_WAIT_TIMEOUT)
            assert proc.pid == pid_before


@pytest.mark.asyncio
async def test_host_ota_rejects_garbage(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
    integration_test_dir,
) -> None:
    """Bogus payload is rejected and the device keeps running."""
    api_port, api_socket = reserved_tcp_port
    with _reserve_port() as (ota_port, ota_socket):
        yaml_config = yaml_config.replace("__OTA_PORT__", str(ota_port))
        config_path = await write_yaml_config(yaml_config)
        binary_path = await compile_esphome(config_path)

        # 192 bytes that are neither ELF nor Mach-O.
        bogus_path = integration_test_dir / "bogus.bin"
        bogus_path.write_bytes(b"NOT-AN-EXECUTABLE-AT-ALL" * 8)

        api_socket.close()
        ota_socket.close()

        async with run_binary(binary_path) as (proc, _lines):
            await _wait_for_port(LOCALHOST, api_port, PORT_WAIT_TIMEOUT)
            pid_before = proc.pid

            loop = asyncio.get_running_loop()
            rc, _ = await loop.run_in_executor(
                None, espota2.run_ota, LOCALHOST, ota_port, None, bogus_path
            )
            assert rc == 1

            await asyncio.sleep(0.5)
            assert proc.returncode is None, "process died on rejected OTA"
            assert proc.pid == pid_before

            async with wait_and_connect_api_client(port=api_port) as client:
                info = await client.device_info()
                assert info.name == DEVICE_NAME
