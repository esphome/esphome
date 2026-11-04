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
from dataclasses import dataclass, field
import functools
import logging
from pathlib import Path
import re
import shutil
import socket
import time
import zlib

import pytest

from esphome import espota2
from esphome.helpers import ProgressBar

from .conftest import run_binary, wait_and_connect_api_client
from .const import (
    KEY_ACTIVATION_DELAY,
    LOCALHOST,
    PORT_POLL_INTERVAL,
    PORT_WAIT_TIMEOUT,
    PROVISIONING_PSK,
    ZERO_PSK,
)
from .host_prefs import force_safe_mode
from .log_utils import LineWaiter
from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, CompileFunction, ConfigWriter

_LOGGER = logging.getLogger(__name__)

DEVICE_NAME = "host-ota-test"
API_KEY = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="


_REAL_COMPRESS = zlib.compress


def _corrupt_compress(data: bytes, *args: object, **kwargs: object) -> bytes:
    """Reserved block type in the first header: rejected by the decoder on
    every build, unlike a flipped data bit that may only fail the MD5."""
    out = bytearray(_REAL_COMPRESS(data, *args, **kwargs))
    out[0] |= 0x06
    return bytes(out)


@contextmanager
def _udp_mode(
    monkeypatch: pytest.MonkeyPatch,
    force: bool = True,
    loss: float = 0.0,
    blocked: bool = False,
) -> Generator[list[bool]]:
    """Force the UDP data phase (unless force is False) and drop datagrams both
    ways; yields one entry per upload, True when it used UDP."""
    import random

    rng = random.Random(1)
    used: list[bool] = []
    real_start = espota2._start_udp
    real_send = espota2.UdpChannel._send
    real_ack = espota2.UdpChannel._handle_ack

    def start(*args: object) -> espota2.UdpChannel | None:
        channel = real_start(*args)
        used.append(channel is not None)
        return channel

    def send(self: espota2.UdpChannel, *args: object) -> None:
        if not blocked and rng.random() >= loss:
            real_send(self, *args)

    def ack(self: espota2.UdpChannel, data: bytes) -> None:
        if rng.random() >= loss:
            real_ack(self, data)

    with monkeypatch.context() as m:
        # Loopback answers in about a millisecond; resend lost datagrams
        # sooner so the loss runs do not dominate the test time
        m.setattr(espota2, "UDP_RESEND_INITIAL", 0.05)
        m.setattr(espota2, "UDP_RESEND_MIN", 0.02)
        if force:
            m.setattr(espota2, "UDP_SLOW_EXCHANGE", -1.0)
        if blocked:
            # Nothing answers, so do not wait the full probe
            m.setattr(espota2, "UDP_PROBE_TIMEOUT", 1.0)
        m.setattr(espota2, "_start_udp", start)
        m.setattr(espota2.UdpChannel, "_send", send)
        m.setattr(espota2.UdpChannel, "_handle_ack", ack)
        yield used


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
) -> tuple[int, int, Path, Path]:
    """Reserve an OTA port, compile the fixture with it, and release both
    ports right before the binary is started. Returns the stripped binary to
    run and upload, then the unstripped build for crash backtraces."""
    api_port, api_socket = reserved_tcp_port
    with _reserve_port() as (ota_port, ota_socket):
        config_path = await write_yaml_config(
            yaml_config.replace("__OTA_PORT__", str(ota_port))
        )
        binary_path = await compile_esphome(config_path)
        api_socket.close()
        ota_socket.close()
    return api_port, ota_port, await _strip_debug(binary_path), binary_path


async def _strip_debug(binary_path: Path) -> Path:
    """A debug re-run builds with -g; every upload sends the image, so drop the
    debug info (GNU and Apple strip both take -S)."""
    if (strip := shutil.which("strip")) is None:
        _LOGGER.warning(
            "strip not found; uploading the unstripped binary, which is much slower"
        )
        return binary_path
    stripped = binary_path.with_name(f"{binary_path.name}.stripped")
    proc = await asyncio.create_subprocess_exec(
        strip, "-S", "-o", str(stripped), str(binary_path)
    )
    assert await proc.wait() == 0, "strip failed"
    return stripped


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


REBOOTED = "Rebooting safely"
INFLATED = re.compile(r"Inflated .* bytes from ")
CORRUPT_STREAM = re.compile(r"Inflate err|Inflate overrun|End update err")


@dataclass
class _Device:
    """A running host binary and the checks every successful OTA repeats:
    a safe reboot, the api port back up, and the pid preserved by execv."""

    api_port: int
    ota_port: int
    binary_path: Path
    symbols_path: Path
    proc: asyncio.subprocess.Process | None = None
    waiter: LineWaiter = field(default_factory=LineWaiter)

    def on_log(self, line: str) -> None:
        self.waiter.callback(line)

    @property
    def reboots(self) -> int:
        return self.waiter.count(REBOOTED)

    @property
    def inflates(self) -> int:
        return self.waiter.count(INFLATED)

    async def wait_reboot(self, count: int, timeout: float = 10.0) -> None:
        await self.waiter.wait_count(REBOOTED, count, timeout, "device did not reboot")

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


def _handshake_then_close(port: int, noise_psk: str) -> None:
    """Negotiate and complete the Noise handshake like a key probe, then
    hang up without sending an OTA type."""
    with socket.create_connection((LOCALHOST, port), timeout=5.0) as sock:
        espota2.send_check(sock, espota2.MAGIC_BYTES, "magic bytes")
        _, version = espota2.receive_exactly(sock, 2, "version", espota2.RESPONSE_OK)
        features_to_send = (
            espota2.CLIENT_FEATURE_SUPPORTS_COMPRESSION
            | espota2.CLIENT_FEATURE_SUPPORTS_SHA256_AUTH
            | espota2.CLIENT_FEATURE_SUPPORTS_EXTENDED_PROTOCOL
            | espota2.CLIENT_FEATURE_SUPPORTS_NOISE
        )
        espota2.send_check(sock, features_to_send, "features")
        espota2.receive_exactly(sock, 1, "features", espota2.RESPONSE_FEATURE_FLAGS)
        (features,) = espota2.receive_exactly(sock, 1, "feature flags", None)
        assert features & espota2.SERVER_FEATURE_SUPPORTS_NOISE
        prologue = (
            espota2.NOISE_PROLOGUE_INIT
            + bytes(espota2.MAGIC_BYTES)
            + bytes([espota2.RESPONSE_OK, version, features_to_send])
            + bytes([espota2.RESPONSE_FEATURE_FLAGS, features])
        )
        noise = espota2.NoiseSocketWrapper(sock, noise_psk, prologue)
        noise.do_handshake()
        espota2.receive_exactly(noise, 1, "auth", espota2.RESPONSE_AUTH_OK)


async def _provision_key(
    dev: _Device, api_client_connected: APIClientConnectedFactory
) -> None:
    """Provision PROVISIONING_PSK over the api and wait for it to activate."""
    async with api_client_connected(port=dev.api_port, noise_psk=ZERO_PSK) as client:
        assert await client.noise_encryption_set_key(PROVISIONING_PSK) is True
    await asyncio.sleep(KEY_ACTIVATION_DELAY)


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
    async with run_binary(
        dev.binary_path, symbols_path=dev.symbols_path, line_callback=dev.on_log
    ) as (
        proc,
        _lines,
    ):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)
        async with wait_and_connect_api_client(port=dev.api_port) as client:
            info_before = await assert_device_name(client, DEVICE_NAME)

        await dev.ota(None, None, "espota2 reported failure")
        assert dev.waiter.count("OTA staged at")

        async with wait_and_connect_api_client(port=dev.api_port) as client:
            await assert_device_name(client, info_before.name)

        # Second OTA: catches FD_CLOEXEC regressions (EADDRINUSE on rebind).
        await dev.ota(None, None, "second OTA failed -- listener leaked across execv")


@pytest.mark.asyncio
async def test_host_ota_deflate_rtl8710b_bit(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The device inflates an upload asked for with the RTL8710B deflate bit."""
    dev = _Device(
        *await _build(
            yaml_config, write_yaml_config, compile_esphome, reserved_tcp_port
        )
    )
    monkeypatch.setattr(espota2, "_is_rtl8710b_image", lambda image: True)

    async with run_binary(dev.binary_path, line_callback=dev.on_log) as (
        proc,
        _lines,
    ):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)
        await dev.ota(None, None, "RTL8710B deflate upload failed")
        assert dev.inflates == 1, "device ignored the RTL8710B deflate offer"


@pytest.mark.asyncio
async def test_host_ota_deflate(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Deflate is negotiated by default, an old client gets an uncompressed
    upload, and a corrupt stream is rejected without taking the device down."""
    dev = _Device(
        *await _build(
            yaml_config, write_yaml_config, compile_esphome, reserved_tcp_port
        )
    )

    def overlong_compress(data: bytes, *args: object, **kwargs: object) -> bytes:
        """A stream that inflates past the size the client announced."""
        return _REAL_COMPRESS(data + bytes(8192), *args, **kwargs)

    async with run_binary(
        dev.binary_path, symbols_path=dev.symbols_path, line_callback=dev.on_log
    ) as (
        proc,
        _lines,
    ):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)

        # Default: the host backend cannot store gzip, so the CLI sends deflate
        await dev.ota(None, None, "deflate upload failed")
        assert dev.inflates == 1, "device did not inflate the upload"

        # A client that does not offer deflate is served uncompressed
        with monkeypatch.context() as m:
            m.setattr(espota2, "CLIENT_FEATURE_SUPPORTS_DEFLATE", 0)
            await dev.ota(None, None, "uncompressed upload failed")
        assert dev.inflates == 1, "device inflated without a client offer"

        # A corrupt stream fails the upload and leaves the device running
        with monkeypatch.context() as m:
            m.setattr(zlib, "compress", _corrupt_compress)
            await dev.refused_ota(None, None, "corrupt deflate stream was accepted")
        # A corrupt stream is caught by the decoder, by the size check or by
        # the MD5 at the end, depending on where the damage lands
        assert dev.waiter.count(CORRUPT_STREAM), (
            "device did not report the corrupt stream"
        )

        # So does a stream that inflates past the announced image size
        overruns = dev.waiter.count("Inflate overrun")
        with monkeypatch.context() as m:
            m.setattr(zlib, "compress", overlong_compress)
            await dev.refused_ota(None, None, "overlong deflate stream was accepted")
        assert dev.waiter.count("Inflate overrun") > overruns, (
            "device wrote past the announced size"
        )

        # and it still takes a good upload afterwards
        await dev.ota(None, None, "upload after a rejected stream failed")
        assert dev.inflates == 2

        # Localhost is clean, so the client stays on TCP
        with _udp_mode(monkeypatch, force=False) as used:
            await dev.ota(None, None, "TCP upload failed")
        assert used == [False], "a clean link asked for UDP"

        # A lossy link moves the data phase to UDP and completes through loss
        with _udp_mode(monkeypatch, loss=0.2) as used:
            await dev.ota(None, None, "UDP upload with loss failed")
        assert used == [True], "the upload did not use UDP"
        assert dev.inflates == 4

        # Blocked UDP falls back to TCP
        with _udp_mode(monkeypatch, blocked=True) as used:
            await dev.ota(None, None, "fallback to TCP failed")
        assert used == [False], "blocked UDP did not fall back to TCP"

        # A device error mid upload surfaces at once, not as a 160 s timeout
        with _udp_mode(monkeypatch) as used, monkeypatch.context() as m:
            m.setattr(zlib, "compress", _corrupt_compress)
            async with asyncio.timeout(30):
                await dev.refused_ota(None, None, "corrupt stream over UDP accepted")
        assert used == [True]


@pytest.mark.asyncio
async def test_host_ota_encrypted(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A client that leaves right after the handshake, as a key probe does,
    is a clean close, not an OTA error; a plaintext upload is refused; an
    encrypted self-OTA succeeds."""
    pytest.importorskip("aioesphomeapi.noise")
    dev = _Device(
        *await _build(
            yaml_config, write_yaml_config, compile_esphome, reserved_tcp_port
        )
    )
    async with run_binary(
        dev.binary_path, symbols_path=dev.symbols_path, line_callback=dev.on_log
    ) as (proc, lines):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)
        await asyncio.get_running_loop().run_in_executor(
            None, _handshake_then_close, dev.ota_port, API_KEY
        )
        # The error path logs its warning instead of this line, never after it
        await _wait_for_line(lines, "Client left after the handshake")
        assert not [line for line in lines if "[W][esphome.ota" in line]
        await dev.refused_ota(
            None, None, "plaintext upload to an encrypted device must fail"
        )
        await dev.ota(None, API_KEY, "encrypted OTA reported failure")
        # Noise frames ride the UDP stream unchanged, through loss
        with _udp_mode(monkeypatch, loss=0.2) as used:
            await dev.ota(None, API_KEY, "encrypted UDP upload failed")
        assert used == [True]


@pytest.mark.asyncio
async def test_host_ota_encrypted_safe_mode(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
) -> None:
    """Safe mode never constructs the api server, so an encrypted OTA with the
    api key has to run on the ota component's own copy of that key."""
    pytest.importorskip("aioesphomeapi.noise")
    dev = _Device(
        *await _build(
            yaml_config, write_yaml_config, compile_esphome, reserved_tcp_port
        )
    )
    # The api port never opens in safe mode, so wait for the log line instead
    force_safe_mode(DEVICE_NAME)
    async with run_binary(
        dev.binary_path, symbols_path=dev.symbols_path, line_callback=dev.on_log
    ) as (proc, lines):
        dev.proc = proc
        await _wait_for_line(lines, "SAFE MODE IS ACTIVE", PORT_WAIT_TIMEOUT)
        await _wait_for_port(LOCALHOST, dev.ota_port, PORT_WAIT_TIMEOUT)
        # The safe mode boot clears the counter, so the re-exec boots normally
        await dev.ota(None, API_KEY, "encrypted OTA in safe mode reported failure")


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
    async with run_binary(
        dev.binary_path, symbols_path=dev.symbols_path, line_callback=dev.on_log
    ) as (proc, lines):
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
    async with run_binary(
        dev.binary_path, symbols_path=dev.symbols_path, line_callback=dev.on_log
    ) as (proc, lines):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)
        await _wait_for_line(lines, "once the api key is provisioned")

        await dev.ota(
            None, None, "plaintext upload to an unprovisioned device must succeed"
        )

        await _provision_key(dev, api_client_connected)

        key = PROVISIONING_PSK.decode()
        await dev.ota(
            None, key, "encrypted upload with the provisioned key must succeed"
        )
        await dev.ota(None, key, "the key loaded at boot must feed the OTA offer")
        await dev.ota(None, None, "plaintext must stay accepted on an offering device")


@pytest.mark.asyncio
async def test_host_ota_provisioned_api_key_safe_mode(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Safe mode never constructs the api server, so the OTA has to load the
    provisioned key from preferences itself to keep encrypting there."""
    pytest.importorskip("aioesphomeapi.noise")
    dev = _Device(
        *await _build(
            yaml_config, write_yaml_config, compile_esphome, reserved_tcp_port
        )
    )
    async with run_binary(
        dev.binary_path, symbols_path=dev.symbols_path, line_callback=dev.on_log
    ) as (proc, _lines):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)
        await _provision_key(dev, api_client_connected)

    # The saved key is already on disk; a host reboot outside an OTA just
    # exits, so safe mode takes a second start
    force_safe_mode(DEVICE_NAME)
    key = PROVISIONING_PSK.decode()
    async with run_binary(
        dev.binary_path, symbols_path=dev.symbols_path, line_callback=dev.on_log
    ) as (proc, lines):
        dev.proc = proc
        await _wait_for_line(lines, "SAFE MODE IS ACTIVE", PORT_WAIT_TIMEOUT)
        await _wait_for_port(LOCALHOST, dev.ota_port, PORT_WAIT_TIMEOUT)
        await dev.ota(
            None,
            key,
            "encrypted upload with the provisioned key must succeed in safe mode",
        )
        # The re-exec boots normally and the api reads the same record
        async with api_client_connected(port=dev.api_port, noise_psk=key):
            pass


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

    async with run_binary(dev.binary_path, symbols_path=dev.symbols_path) as (
        proc,
        _lines,
    ):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)
        pid_before = proc.pid
        rc = await _run_ota(dev.ota_port, None, bogus_path, None)
        assert rc == 1
        await asyncio.sleep(0.5)
        assert proc.returncode is None, "process died on rejected OTA"
        assert proc.pid == pid_before

        async with wait_and_connect_api_client(port=dev.api_port) as client:
            await assert_device_name(client, DEVICE_NAME)


class _SlowProgressBar(ProgressBar):
    """Pauses once mid upload, longer than the device's progress interval, so
    the device reports progress at least once."""

    _paused = False

    def update(self, progress: float) -> None:
        if progress >= 0.5 and not self._paused:
            self._paused = True
            time.sleep(1.5)
        super().update(progress)


AUTOMATION_LINE = re.compile(r"AUTOMATION ([^\x1b]+)")
# Pinned on purpose: both on_state_change automations run before on_begin, in config order
BEGIN_LINES = ["state 1", "second state 1", "begin"]


def _progress_values(lines: list[str]) -> list[float]:
    """Each progress report is both state automations, then on_progress."""
    assert len(lines) % 3 == 0, lines
    values: list[float] = []
    for i in range(0, len(lines), 3):
        assert lines[i : i + 2] == ["state 2", "second state 2"], lines
        values.append(float(lines[i + 2].removeprefix("progress ")))
    return values


@pytest.mark.asyncio
async def test_host_ota_automations(
    yaml_config: str,
    write_yaml_config: ConfigWriter,
    compile_esphome: CompileFunction,
    reserved_tcp_port: tuple[int, socket.socket],
    integration_test_dir: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Each on_* automation fires for its state with its argument, and every
    automation on one key fires."""
    dev = _Device(
        *await _build(
            yaml_config, write_yaml_config, compile_esphome, reserved_tcp_port
        )
    )
    bogus_path = integration_test_dir / "bogus.bin"
    bogus_path.write_bytes(b"NOT-AN-EXECUTABLE-AT-ALL" * 8)

    def automation_lines() -> list[str]:
        return [
            match.group(1)
            for line in dev.waiter.lines
            if (match := AUTOMATION_LINE.search(line))
        ]

    async with run_binary(
        dev.binary_path, symbols_path=dev.symbols_path, line_callback=dev.on_log
    ) as (proc, _lines):
        dev.proc = proc
        await _wait_for_port(LOCALHOST, dev.api_port, PORT_WAIT_TIMEOUT)

        # A rejected image: begin, then error with the response code
        rc = await _run_ota(dev.ota_port, None, bogus_path, None)
        assert rc == 1
        await dev.waiter.wait_count(
            "AUTOMATION error", 1, 5.0, "error automation did not fire"
        )
        lines = automation_lines()
        assert lines[:3] == BEGIN_LINES
        assert lines[-3:] == [
            "state 4",
            "second state 4",
            f"error {espota2.RESPONSE_ERROR_UPDATE_END}",
        ]
        assert all(0.0 < v <= 100.0 for v in _progress_values(lines[3:-3]))

        # A good image: begin, progress, end, then the reboot
        dev.waiter.lines.clear()
        with monkeypatch.context() as m:
            m.setattr(espota2, "ProgressBar", _SlowProgressBar)
            await dev.ota(None, None, "espota2 reported failure")
        lines = automation_lines()
        assert lines[:3] == BEGIN_LINES
        assert lines[-3:] == ["state 0", "second state 0", "end"]
        values = _progress_values(lines[3:-3])
        assert any(0.0 < v < 100.0 for v in values), "no progress mid upload"
        assert "abort" not in lines
