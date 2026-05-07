"""Integration tests for OTA on the host platform.

Exercises the full native OTA wire protocol end-to-end against a real host
binary: handshake, magic, features, MD5 verification, chunked transfer, and
the post-end re-exec. The host backend stages the new binary to a sibling
file, validates the executable header against the running exe, atomically
renames it into place, and arms `arch_restart()` to `execv` instead of
`exit()`. Re-exec preserves the pid -- the test asserts this to prove the
process really swapped binaries rather than restarting via supervisor.
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
            # safe_reboot logs this line just before arch_restart() execs.
            if not rebooted.done() and "Rebooting safely" in line:
                rebooted.set_result(True)

        async with run_binary(binary_path, line_callback=on_log) as (proc, _lines):
            await _wait_for_port(LOCALHOST, api_port, PORT_WAIT_TIMEOUT)
            pid_before = proc.pid
            async with wait_and_connect_api_client(port=api_port) as client:
                info_before = await client.device_info()
                assert info_before.name == DEVICE_NAME

            # Run OTA in a thread -- espota2 uses blocking sockets. Upload the
            # very binary that is currently running; on success the device
            # validates, swaps it in, and execv's itself.
            rc, _ = await loop.run_in_executor(
                None, espota2.run_ota, LOCALHOST, ota_port, None, binary_path
            )
            assert rc == 0, "espota2 reported failure"

            # The device must report it staged the binary and is rebooting.
            await asyncio.wait_for(ota_staged, timeout=10.0)
            await asyncio.wait_for(rebooted, timeout=10.0)

            # Wait for the new exec to rebind the API port. The old listener
            # closes during safe_reboot(); the new exec re-opens it.
            await _wait_for_port(LOCALHOST, api_port, PORT_WAIT_TIMEOUT)

            # execv preserves pid. If the process had died and been respawned
            # by something external, the pid would differ.
            assert proc.returncode is None, "process exited instead of execing"
            assert proc.pid == pid_before

            async with wait_and_connect_api_client(port=api_port) as client:
                info_after = await client.device_info()
                assert info_after.name == DEVICE_NAME
                assert info_after.name == info_before.name

            # Run a second OTA against the post-exec instance. This catches
            # regressions where the listening socket leaks across execv and
            # the new image can't bind (errno EADDRINUSE) -- without
            # FD_CLOEXEC the second OTA's bind would fail and rc would be 1.
            rc, _ = await loop.run_in_executor(
                None, espota2.run_ota, LOCALHOST, ota_port, None, binary_path
            )
            assert rc == 0, (
                "second OTA failed -- listening socket likely leaked across execv"
            )
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
    """Truncated/garbage payload must be rejected and the device must keep running."""
    api_port, api_socket = reserved_tcp_port
    with _reserve_port() as (ota_port, ota_socket):
        yaml_config = yaml_config.replace("__OTA_PORT__", str(ota_port))
        config_path = await write_yaml_config(yaml_config)
        binary_path = await compile_esphome(config_path)

        # A payload that passes nothing: 200 bytes that are neither valid ELF
        # nor Mach-O. The device must reject it after the MD5 checks pass.
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
            # espota2 returns 1 on backend rejection.
            assert rc == 1

            # Device must still be alive and answering.
            await asyncio.sleep(0.5)
            assert proc.returncode is None, "process died on rejected OTA"
            assert proc.pid == pid_before

            async with wait_and_connect_api_client(port=api_port) as client:
                info = await client.device_info()
                assert info.name == DEVICE_NAME
