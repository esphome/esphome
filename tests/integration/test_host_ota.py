"""End-to-end OTA tests on the host platform.

Exercises the native OTA protocol against a real host binary, then asserts
pid is preserved across the post-OTA execv. A second OTA on the post-exec
instance covers the FD_CLOEXEC path.
"""

from __future__ import annotations

import asyncio
import base64
from collections.abc import Generator
from contextlib import contextmanager
from dataclasses import dataclass
import functools
from pathlib import Path
import socket

import pytest

from esphome import espota2

from .conftest import run_binary, wait_and_connect_api_client
from .const import (
    KEY_ACTIVATION_DELAY,
    LOCALHOST,
    PORT_POLL_INTERVAL,
    PORT_WAIT_TIMEOUT,
    PROVISIONING_PSK,
    ZERO_PSK,
)
from .types import APIClientConnectedFactory, CompileFunction, ConfigWriter

DEVICE_NAME = "host-ota-test"
API_KEY = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="


@contextmanager
def _reserve_port() -> Generator[tuple[int, socket.socket]]:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("", 0))
    try:
        yield s.getsockname()[1], s
    finally:
        s.close()


async def _wait_for_line(lines: list[str], needle: str, timeout: float = 5.0) -> None:
    """The config dump prints after every setup, a little after the api port
    opens, so wait for it rather than assert on the lines seen so far."""
    async with asyncio.timeout(timeout):
        while not any(needle in line for line in lines):
            await asyncio.sleep(PORT_POLL_INTERVAL)


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


async def _build(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> tuple[int, int, Path]:
    """Reserve an OTA port, compile the fixture with it, and release both
    ports right before the binary is started."""
    api_port, api_socket = reserved_tcp_port
    with _reserve_port() as (ota_port, ota_socket):
        config_path = await write_yaml_config(
            yaml_config.replace("__OTA_PORT__", str(ota_port))
        )
        binary_path = await compile_esphome(config_path)
        api_socket.close()
        ota_socket.close()
    return api_port, ota_port, binary_path


async def _run_ota(
    ota_port: int,
    password: str | None,
    binary_path: Path,
    noise_psk: str | None,
    plaintext_fallback: bool = False,
) -> int:
    """espota2 is blocking; run it in the executor and return its exit code."""
    rc, _ = await asyncio.get_running_loop().run_in_executor(
        None,
        functools.partial(
            espota2.run_ota,
            LOCALHOST,
            ota_port,
            password,
            binary_path,
            noise_psk=noise_psk,
            plaintext_fallback=plaintext_fallback,
        ),
    )
    return rc


@dataclass
class _Device:
    """A running host binary and the checks every successful OTA repeats:
    a safe reboot, the api port back up, and the pid preserved by execv."""

    api_port: int
    ota_port: int
    binary_path: Path
    proc: asyncio.subprocess.Process | None = None
    reboots: int = 0

    def __post_init__(self) -> None:
        self._rebooted = asyncio.Event()

    def on_log(self, line: str) -> None:
        if "Rebooting safely" in line:
            self.reboots += 1
            self._rebooted.set()

    async def wait_reboot(self, count: int, timeout: float = 10.0) -> None:
        async with asyncio.timeout(timeout):
            while self.reboots < count:
                self._rebooted.clear()
                await self._rebooted.wait()

    async def ota(
        self,
        password: str | None,
        noise_psk: str | None,
        msg: str,
        plaintext_fallback: bool = False,
    ) -> None:
        """Upload, then expect the re-exec with the pid preserved."""
        pid_before = self.proc.pid
        expected_reboots = self.reboots + 1
        rc = await _run_ota(
            self.ota_port, password, self.binary_path, noise_psk, plaintext_fallback
        )
        assert rc == 0, msg
        await self.wait_reboot(expected_reboots)
        await _wait_for_port(LOCALHOST, self.api_port, PORT_WAIT_TIMEOUT)
        assert self.proc.returncode is None, "process exited instead of execing"
        assert self.proc.pid == pid_before

    async def refused_ota(
        self, password: str | None, noise_psk: str | None, msg: str
    ) -> None:
        """Upload must fail and the device must keep running."""
        rc = await _run_ota(self.ota_port, password, self.binary_path, noise_psk)
        assert rc == 1, msg
        await asyncio.sleep(0.5)
        assert self.proc.returncode is None, "process died on rejected OTA"


@pytest.mark.asyncio
async def test_host_ota_self_update(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> None:
    """Self-OTA: upload the running binary back to itself, expect re-exec."""
    dev = _Device(
        *await _build(
            yaml_config, write_yaml_config, compile_esphome, reserved_tcp_port
        )
    )
    staged = asyncio.Event()

    def on_log(line: str) -> None:
        if "OTA staged at" in line:
            staged.set()
        dev.on_log(line)

    async with run_binary(dev.binary_path, line_callback=on_log) as (proc, _lines):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)
        async with wait_and_connect_api_client(port=dev.api_port) as client:
            info_before = await client.device_info()
            assert info_before.name == DEVICE_NAME

        await dev.ota(None, None, "espota2 reported failure")
        assert staged.is_set()

        async with wait_and_connect_api_client(port=dev.api_port) as client:
            info_after = await client.device_info()
            assert info_after.name == info_before.name

        # Second OTA: catches FD_CLOEXEC regressions (EADDRINUSE on rebind).
        await dev.ota(None, None, "second OTA failed -- listener leaked across execv")


@pytest.mark.asyncio
async def test_host_ota_encrypted(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> None:
    """Encrypted self-OTA succeeds; a plaintext upload to the same device fails."""
    pytest.importorskip("aioesphomeapi.noise")
    dev = _Device(
        *await _build(
            yaml_config, write_yaml_config, compile_esphome, reserved_tcp_port
        )
    )
    async with run_binary(dev.binary_path, line_callback=dev.on_log) as (proc, _lines):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)
        await dev.refused_ota(
            None, None, "plaintext upload to an encrypted device must fail"
        )
        await dev.ota(None, API_KEY, "encrypted OTA reported failure")


@pytest.mark.asyncio
async def test_host_ota_api_key_offer_with_password(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """With only an api key the device offers encryption without requiring
    it: the password still guards plaintext uploads, the key alone
    authenticates an encrypted one, and until 2027.3.0 a failed encrypted
    attempt falls back to plaintext."""
    pytest.importorskip("aioesphomeapi.noise")
    wrong_key = base64.b64encode(b"w" * 32).decode()
    dev = _Device(
        *await _build(
            yaml_config, write_yaml_config, compile_esphome, reserved_tcp_port
        )
    )
    async with run_binary(dev.binary_path, line_callback=dev.on_log) as (proc, lines):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)
        await _wait_for_line(lines, "Encryption: offered")

        await dev.refused_ota(
            None, None, "plaintext upload without the password must fail"
        )
        await dev.ota(
            "hunter2", None, "plaintext upload with the password must succeed"
        )
        await dev.ota(None, API_KEY, "encrypted upload with the api key must succeed")

        # Remove before 2027.3.0: a wrong key falls back to plaintext, which
        # the password still guards
        with caplog.at_level("WARNING", logger="esphome.espota2"):
            await dev.ota(
                "hunter2",
                wrong_key,
                "the plaintext retry with the password must succeed",
                plaintext_fallback=True,
            )
        assert any("Retrying in plaintext" in r.message for r in caplog.records)
        await dev.ota(
            None,
            API_KEY,
            "the right api key encrypts without touching the fallback",
            plaintext_fallback=True,
        )


@pytest.mark.asyncio
@pytest.mark.usefixtures("isolated_preferences")
async def test_host_ota_provisioned_api_key(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A key provisioned over the api feeds the OTA offer: plaintext works
    while unprovisioned, the provisioned key encrypts, the key loaded from
    preferences on the next boot keeps encrypting, and plaintext stays
    accepted because only the ota block requires encryption."""
    pytest.importorskip("aioesphomeapi.noise")
    dev = _Device(
        *await _build(
            yaml_config, write_yaml_config, compile_esphome, reserved_tcp_port
        )
    )
    async with run_binary(dev.binary_path, line_callback=dev.on_log) as (proc, lines):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)
        await _wait_for_line(lines, "once the api key is provisioned")

        await dev.ota(
            None, None, "plaintext upload to an unprovisioned device must succeed"
        )

        async with api_client_connected(
            port=dev.api_port, noise_psk=ZERO_PSK
        ) as client:
            assert await client.noise_encryption_set_key(PROVISIONING_PSK) is True
        await asyncio.sleep(KEY_ACTIVATION_DELAY)

        key = PROVISIONING_PSK.decode()
        await dev.ota(
            None, key, "encrypted upload with the provisioned key must succeed"
        )
        await dev.ota(None, key, "the key loaded at boot must feed the OTA offer")
        await dev.ota(None, None, "plaintext must stay accepted on an offering device")


@pytest.mark.asyncio
async def test_host_ota_rejects_garbage(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
    integration_test_dir,
) -> None:
    """Bogus payload is rejected and the device keeps running."""
    dev = _Device(
        *await _build(
            yaml_config, write_yaml_config, compile_esphome, reserved_tcp_port
        )
    )
    # 192 bytes that are neither ELF nor Mach-O.
    bogus_path = integration_test_dir / "bogus.bin"
    bogus_path.write_bytes(b"NOT-AN-EXECUTABLE-AT-ALL" * 8)

    async with run_binary(dev.binary_path) as (proc, _lines):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)
        pid_before = proc.pid
        rc = await _run_ota(dev.ota_port, None, bogus_path, None)
        assert rc == 1
        await asyncio.sleep(0.5)
        assert proc.returncode is None, "process died on rejected OTA"
        assert proc.pid == pid_before

        async with wait_and_connect_api_client(port=dev.api_port) as client:
            info = await client.device_info()
            assert info.name == DEVICE_NAME
