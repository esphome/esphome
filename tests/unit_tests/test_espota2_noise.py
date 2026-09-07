"""Unit tests for encrypted OTA uploads in esphome.espota2.

A fake device implementing the responder side of the wire protocol (via
noiseprotocol, which esphome already has through aioesphomeapi) serves a real
TCP loopback connection, so these exercise the actual handshake, framing, and
cipher interop of the client code. Tests that need the client-side crypto skip
when the installed aioesphomeapi predates the noise module.
"""

from __future__ import annotations

import base64
from collections.abc import Callable
import hashlib
import io
import logging
from pathlib import Path
import socket
import sys
import threading
from typing import Any
from unittest.mock import Mock, patch

import pytest

from esphome import espota2

PSK = base64.b64encode(bytes(range(32))).decode()
OTHER_PSK = base64.b64encode(bytes(range(1, 33))).decode()

MAGIC = bytes(espota2.MAGIC_BYTES)


def _recv_exact(sock: socket.socket, amount: int) -> bytes:
    data = b""
    while len(data) < amount:
        chunk = sock.recv(amount - len(data))
        if not chunk:
            raise ConnectionError("client closed")
        data += chunk
    return data


def _frame(payload: bytes) -> bytes:
    return (
        bytes([espota2.NOISE_FRAME_INDICATOR, len(payload) >> 8, len(payload) & 0xFF])
        + payload
    )


def _send_frame(sock: socket.socket, payload: bytes) -> None:
    sock.sendall(_frame(payload))


def _recv_frame(sock: socket.socket) -> bytes:
    header = _recv_exact(sock, 3)
    assert header[0] == 0x01
    return _recv_exact(sock, (header[1] << 8) | header[2])


class FakeEncryptedDevice(threading.Thread):
    """Responder side of the encrypted OTA wire protocol."""

    def __init__(
        self,
        psk: str = PSK,
        version: int = 2,
        offer_noise: bool = True,
        require_noise: bool = True,
        prologue_features_override: int | None = None,
        connections: int = 1,
        drop_handshakes: int = 0,
    ) -> None:
        super().__init__(daemon=True)
        self.connections = connections
        self.drop_handshakes = drop_handshakes  # hang up mid-handshake this many times
        self.psk = psk
        self.version = version
        self.offer_noise = offer_noise
        self.require_noise = require_noise
        self.prologue_features_override = prologue_features_override
        self.received: bytes | None = None
        self.error: Exception | None = None
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(1)
        self.port = self.listener.getsockname()[1]

    def run(self) -> None:
        try:
            for _ in range(self.connections):
                sock, _ = self.listener.accept()
                sock.settimeout(10)
                with sock:
                    self._serve(sock)
        except Exception as err:  # noqa: BLE001 - surfaced via join_and_check
            self.error = err
        finally:
            self.listener.close()

    def join_and_check(self) -> None:
        self.join(timeout=10)
        assert not self.is_alive(), "fake device did not finish"
        if self.error is not None:
            raise self.error

    def _serve(self, sock: socket.socket) -> None:
        assert _recv_exact(sock, 5) == MAGIC
        sock.sendall(bytes([espota2.RESPONSE_OK, self.version]))
        features = _recv_exact(sock, 1)[0]
        noise_negotiated = bool(
            features & espota2.CLIENT_FEATURE_SUPPORTS_NOISE
            and features & espota2.CLIENT_FEATURE_SUPPORTS_EXTENDED_PROTOCOL
        )
        if self.require_noise and not noise_negotiated:
            sock.sendall(bytes([espota2.RESPONSE_ERROR_ENCRYPTION_REQUIRED]))
            return
        server_flags = espota2.SERVER_FEATURE_SUPPORTS_NOISE if self.offer_noise else 0
        sock.sendall(bytes([espota2.RESPONSE_FEATURE_FLAGS, server_flags]))
        if not (noise_negotiated and self.offer_noise):
            # A device that does not require encryption continues in
            # plaintext whatever the client asked for, like older firmware
            try:
                self._transfer(
                    lambda byte: sock.sendall(bytes([byte])),
                    lambda length: _recv_exact(sock, length),
                    lambda remaining: _recv_exact(
                        sock, min(remaining, espota2.UPLOAD_BLOCK_SIZE)
                    ),
                )
            except ConnectionError:
                # A keyed client without fallback fails closed and hangs up
                if noise_negotiated and not self.offer_noise:
                    return
                raise
            return

        from cryptography.exceptions import InvalidTag
        from noise.connection import NoiseConnection

        prologue_features = (
            features
            if self.prologue_features_override is None
            else self.prologue_features_override
        )
        prologue = (
            espota2.NOISE_PROLOGUE_INIT
            + MAGIC
            + bytes([espota2.RESPONSE_OK, self.version, prologue_features])
            + bytes([espota2.RESPONSE_FEATURE_FLAGS, server_flags])
        )
        proto = NoiseConnection.from_name(b"Noise_NNpsk0_25519_ChaChaPoly_SHA256")
        proto.set_as_responder()
        proto.set_psks(base64.b64decode(self.psk))
        proto.set_prologue(prologue)
        proto.start_handshake()

        msg1 = _recv_frame(sock)
        assert msg1[0] == 0x00
        if self.drop_handshakes > 0:
            self.drop_handshakes -= 1
            return  # a transport fault: the socket closes with no reply
        try:
            proto.read_message(msg1[1:])
        except InvalidTag:
            _send_frame(sock, b"\x01" + espota2.NOISE_MAC_FAILURE_REASON.encode())
            return
        _send_frame(sock, b"\x00" + bytes(proto.write_message()))

        def send_byte(byte: int) -> None:
            _send_frame(sock, proto.encrypt(bytes([byte])))

        def recv_unit(length: int) -> bytes:
            plaintext = proto.decrypt(_recv_frame(sock))
            assert len(plaintext) == length, "control units must be one per frame"
            return plaintext

        def recv_data(_remaining: int) -> bytes:
            plaintext = proto.decrypt(_recv_frame(sock))
            assert 0 < len(plaintext) <= espota2.NOISE_MAX_PLAINTEXT
            return plaintext

        self._transfer(send_byte, recv_unit, recv_data)

    def _transfer(
        self,
        send_byte: Callable[[int], None],
        recv_unit: Callable[[int], bytes],
        recv_data: Callable[[int], bytes],
    ) -> None:
        """The post-handshake exchange, identical over both transports."""
        send_byte(espota2.RESPONSE_AUTH_OK)
        recv_unit(1)  # ota type
        size = int.from_bytes(recv_unit(4), "big")
        send_byte(espota2.RESPONSE_UPDATE_PREPARE_OK)
        md5_hex = recv_unit(32)
        send_byte(espota2.RESPONSE_BIN_MD5_OK)

        received = b""
        acked = 0
        while len(received) < size:
            received += recv_data(size - len(received))
            if self.version >= espota2.OTA_VERSION_2_0:
                while acked + espota2.UPLOAD_BLOCK_SIZE <= len(received) or (
                    len(received) == size and acked < size
                ):
                    send_byte(espota2.RESPONSE_CHUNK_OK)
                    acked += espota2.UPLOAD_BLOCK_SIZE
        assert hashlib.md5(received).hexdigest().encode() == md5_hex
        send_byte(espota2.RESPONSE_RECEIVE_OK)
        send_byte(espota2.RESPONSE_UPDATE_END_OK)
        assert recv_unit(1) == bytes([espota2.RESPONSE_OK])
        self.received = received


def _upload(
    device: FakeEncryptedDevice,
    firmware: bytes,
    noise_psk: str | None,
    plaintext_fallback: bool = False,
) -> None:
    device.start()
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)
    sock.connect(("127.0.0.1", device.port))
    try:
        espota2.perform_ota(
            sock,
            None,
            io.BytesIO(firmware),
            Path("firmware.bin"),
            noise_psk=noise_psk,
            plaintext_fallback=plaintext_fallback,
        )
    finally:
        sock.close()


def _run_ota(
    device: FakeEncryptedDevice, firmware: bytes, tmp_path: Path, noise_psk: str
) -> int:
    """Drive the retry loop, which is where the plaintext fallback reconnects."""
    path = tmp_path / "firmware.bin"
    path.write_bytes(firmware)
    device.start()
    rc, _ = espota2.run_ota(
        "127.0.0.1",
        device.port,
        None,
        path,
        noise_psk=noise_psk,
        plaintext_fallback=True,
    )
    return rc


def test_encrypted_upload_success() -> None:
    """A full encrypted v2 upload spanning several 8192-byte blocks."""
    pytest.importorskip("aioesphomeapi.noise")
    firmware = bytes(range(256)) * 80  # 20480 bytes, crosses chunk-ack boundaries
    device = FakeEncryptedDevice()
    with patch("time.sleep"):
        _upload(device, firmware, PSK)
    device.join_and_check()
    assert device.received == firmware


def test_encrypted_upload_version_1() -> None:
    """Version 1 protocol (no chunk acks) works through the noise transport."""
    pytest.importorskip("aioesphomeapi.noise")
    firmware = b"v1 firmware image" * 100
    device = FakeEncryptedDevice(version=1)
    with patch("time.sleep"):
        _upload(device, firmware, PSK)
    device.join_and_check()
    assert device.received == firmware


def test_wrong_key_fails_with_clear_error() -> None:
    """A key mismatch surfaces the device's handshake reject readably."""
    pytest.importorskip("aioesphomeapi.noise")
    device = FakeEncryptedDevice(psk=OTHER_PSK)
    with pytest.raises(espota2.OTAError, match="encryption key correct"):
        _upload(device, b"firmware", PSK)
    device.join_and_check()


def test_tampered_negotiation_breaks_handshake() -> None:
    """A negotiation byte differing between the sides breaks the prologue MAC."""
    pytest.importorskip("aioesphomeapi.noise")
    device = FakeEncryptedDevice(
        prologue_features_override=espota2.CLIENT_FEATURE_SUPPORTS_EXTENDED_PROTOCOL
    )
    with pytest.raises(espota2.OTAError, match="encryption key correct"):
        _upload(device, b"firmware", PSK)
    device.join_and_check()


def test_client_fails_closed_when_device_lacks_encryption() -> None:
    """With a key configured, a device not offering noise aborts the upload."""
    device = FakeEncryptedDevice(offer_noise=False, require_noise=False)
    with pytest.raises(espota2.OTAError, match="refusing to send the image"):
        _upload(device, b"firmware", PSK)
    device.join_and_check()


# Remove before 2027.3.0
def test_fallback_when_device_does_not_offer(caplog: pytest.LogCaptureFixture) -> None:
    """The api key is tried opportunistically; an older device that cannot
    encrypt still gets its update, with a warning."""
    firmware = b"firmware"
    device = FakeEncryptedDevice(offer_noise=False, require_noise=False)
    with patch("time.sleep"), caplog.at_level(logging.WARNING):
        _upload(device, firmware, PSK, plaintext_fallback=True)
    device.join_and_check()
    assert device.received == firmware
    assert any("fallback is removed in 2027.3.0" in r.message for r in caplog.records)


# Remove before 2027.3.0
@pytest.mark.parametrize(
    ("device_kwargs", "expected_rc", "fell_back"),
    [
        # A wrong key against an offering device reconnects in plaintext
        ({"psk": OTHER_PSK, "require_noise": False, "connections": 2}, 0, True),
        # The plaintext retry is refused by a device that requires encryption
        ({"psk": OTHER_PSK, "require_noise": True, "connections": 2}, 1, True),
        # A dropped connection inside the handshake is retried encrypted
        ({"require_noise": False, "connections": 2, "drop_handshakes": 1}, 0, False),
        # A second transport fault inside the handshake falls back
        ({"require_noise": False, "connections": 3, "drop_handshakes": 2}, 0, True),
    ],
    ids=["wrong_key", "wrong_key_required", "one_fault", "two_faults"],
)
def test_fallback_through_the_retry_loop(
    caplog: pytest.LogCaptureFixture,
    tmp_path: Path,
    device_kwargs: dict[str, Any],
    expected_rc: int,
    fell_back: bool,
) -> None:
    pytest.importorskip("aioesphomeapi.noise")
    firmware = b"firmware"
    device = FakeEncryptedDevice(**device_kwargs)
    with patch("time.sleep"), caplog.at_level(logging.WARNING):
        rc = _run_ota(device, firmware, tmp_path, PSK)
    device.join_and_check()
    assert rc == expected_rc
    assert (device.received == firmware) is (expected_rc == 0)
    assert (
        any("Retrying in plaintext" in r.message for r in caplog.records) is fell_back
    )
    if expected_rc == 1:
        assert any("requires an encrypted OTA" in r.message for r in caplog.records)


def test_plaintext_client_gets_encryption_required_error() -> None:
    """A client without a key gets the device's 0x94 error message."""
    device = FakeEncryptedDevice()
    with pytest.raises(espota2.OTAError, match="requires an encrypted OTA"):
        _upload(device, b"firmware", None)
    device.join_and_check()


def test_missing_aioesphomeapi_noise_module_message() -> None:
    """An aioesphomeapi without the noise module produces a clear error."""
    with (
        patch.dict(sys.modules, {"aioesphomeapi.noise": None}),
        pytest.raises(espota2.OTAError, match="requires a newer aioesphomeapi"),
    ):
        espota2.NoiseSocketWrapper(Mock(), PSK, b"prologue")


class ScriptedSocket:
    """Serves scripted recv chunks; b"" means the peer closed."""

    def __init__(self, *chunks: bytes | Exception) -> None:
        self.chunks = list(chunks)
        self.sent: list[bytes] = []

    def sendall(self, data: bytes) -> None:
        self.sent.append(data)

    def settimeout(self, timeout: float) -> None:
        pass

    def recv(self, amount: int) -> bytes:
        if not self.chunks:
            return b""
        chunk = self.chunks[0]
        if isinstance(chunk, Exception):
            self.chunks.pop(0)
            raise chunk
        take, rest = chunk[:amount], chunk[amount:]
        if rest:
            self.chunks[0] = rest
        else:
            self.chunks.pop(0)
        return take


def _wrapper(*chunks: bytes | Exception) -> espota2.NoiseSocketWrapper:
    pytest.importorskip("aioesphomeapi.noise")
    return espota2.NoiseSocketWrapper(ScriptedSocket(*chunks), PSK, b"prologue")


def test_wrapper_rejects_malformed_psk() -> None:
    pytest.importorskip("aioesphomeapi.noise")
    with pytest.raises(espota2.OTAError, match="Invalid OTA encryption key"):
        espota2.NoiseSocketWrapper(ScriptedSocket(), "not-base64!!!", b"prologue")


def test_handshake_socket_error_is_network_error() -> None:
    wrapper = _wrapper(OSError("boom"))
    with pytest.raises(espota2.OTANetworkError, match="noise handshake"):
        wrapper.do_handshake()


def test_handshake_closed_at_frame_boundary() -> None:
    wrapper = _wrapper()
    with pytest.raises(espota2.OTANetworkError, match="closed connection during"):
        wrapper.do_handshake()


def test_handshake_reject_with_other_reason() -> None:
    wrapper = _wrapper(_frame(b"\x01Handshake error"))
    with pytest.raises(
        espota2.OTAError, match="rejected the noise handshake: Handshake error"
    ):
        wrapper.do_handshake()


def test_handshake_garbage_second_message() -> None:
    """A valid-looking point with a garbage MAC fails cleanly."""
    wrapper = _wrapper(_frame(b"\x00" + bytes(range(48))))
    with pytest.raises(
        espota2.OTAError, match="handshake failed; is the OTA encryption key"
    ):
        wrapper.do_handshake()


def test_handshake_invalid_curve_point() -> None:
    """An all-zero x25519 point is rejected as a clean error, not a crash."""
    wrapper = _wrapper(_frame(b"\x00" + bytes(48)))
    with pytest.raises(
        espota2.OTAError, match="handshake failed; is the OTA encryption key"
    ):
        wrapper.do_handshake()


def test_recv_closed_at_frame_boundary_returns_empty() -> None:
    wrapper = _wrapper()
    assert wrapper.recv(1) == b""


def test_recv_corrupt_frame_is_retryable_network_error() -> None:
    from cryptography.exceptions import InvalidTag

    wrapper = _wrapper(_frame(b"ciphertext"))
    wrapper._decrypt = Mock(decrypt=Mock(side_effect=InvalidTag()))
    with pytest.raises(espota2.OTANetworkError, match="decryption failed"):
        wrapper.recv(1)


def test_wrapper_blocks_unencrypted_socket_methods() -> None:
    """Byte-moving socket methods must not bypass the encrypted transport."""
    wrapper = _wrapper()
    # The harmless socket controls pass through to the wrapped socket
    wrapper._sock = Mock()
    wrapper.settimeout(1)
    wrapper._sock.settimeout.assert_called_once_with(1)
    wrapper.setsockopt(6, 1, 1)
    wrapper._sock.setsockopt.assert_called_once_with(6, 1, 1)
    wrapper.close()
    wrapper._sock.close.assert_called_once_with()
    with pytest.raises(AttributeError):
        _ = wrapper.send
    with pytest.raises(AttributeError):
        _ = wrapper.recv_into


def test_recv_empty_plaintext_frame_is_protocol_error() -> None:
    """A MAC-only frame decrypts to nothing; b'' from recv must mean close."""
    wrapper = _wrapper(_frame(bytes(16)))
    wrapper._decrypt = Mock(decrypt=Mock(return_value=b""))
    with pytest.raises(espota2.OTANetworkError, match="empty noise frame"):
        wrapper.recv(1)


def test_recv_frame_bad_indicator_is_retryable() -> None:
    wrapper = _wrapper(b"\x02\x00\x01x")
    with pytest.raises(espota2.OTANetworkError, match="Bad noise frame indicator"):
        wrapper._recv_frame()


def test_recv_frame_zero_length_is_retryable() -> None:
    wrapper = _wrapper(bytes([espota2.NOISE_FRAME_INDICATOR, 0, 0]))
    with pytest.raises(espota2.OTANetworkError, match="empty noise frame"):
        wrapper._recv_frame()


def test_perform_ota_blank_key_refuses_plaintext() -> None:
    with pytest.raises(espota2.OTAError, match="empty OTA encryption key"):
        espota2.perform_ota(
            ScriptedSocket(), None, io.BytesIO(b"x"), Path("f.bin"), noise_psk=""
        )


def test_recv_exact_closed_mid_frame() -> None:
    wrapper = _wrapper(_frame(b"partial")[:5])
    with pytest.raises(OSError, match="closed inside a noise frame"):
        wrapper._recv_frame()


def test_recv_serves_buffered_plaintext_without_new_frame() -> None:
    """A second recv drains the decrypted buffer without reading another frame."""
    wrapper = _wrapper(_frame(b"ciphertext"))
    wrapper._decrypt = Mock(decrypt=Mock(return_value=b"AB"))
    assert wrapper.recv(1) == b"A"  # reads and decrypts one frame
    assert wrapper.recv(1) == b"B"  # served from the buffer, no new frame
    wrapper._decrypt.decrypt.assert_called_once()
