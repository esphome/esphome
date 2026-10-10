"""Unit tests for esphome.espota2 module."""

from __future__ import annotations

from collections.abc import Generator
import gzip
import hashlib
import io
import itertools
import logging
from pathlib import Path
import socket
import struct
from typing import Self
from unittest.mock import Mock, call, patch
import zlib

import pytest
from pytest import CaptureFixture

from esphome import espota2
from esphome.core import EsphomeError

# Test constants
MOCK_MD5_CNONCE = "a" * 32  # Mock 32-char hex string from secrets.token_hex(16)
MOCK_SHA256_CNONCE = "b" * 64  # Mock 64-char hex string from secrets.token_hex(32)
MOCK_MD5_NONCE = b"12345678901234567890123456789012"  # 32 char nonce for MD5
MOCK_SHA256_NONCE = b"1234567890123456789012345678901234567890123456789012345678901234"  # 64 char nonce for SHA256


@pytest.fixture
def mock_socket() -> Mock:
    """Create a mock socket for testing."""
    socket_mock = Mock()
    socket_mock.close = Mock()
    socket_mock.recv = Mock()
    socket_mock.sendall = Mock()
    socket_mock.settimeout = Mock()
    socket_mock.connect = Mock()
    socket_mock.setsockopt = Mock()
    return socket_mock


@pytest.fixture
def mock_file() -> io.BytesIO:
    """Create a mock firmware file for testing."""
    return io.BytesIO(b"firmware content here")


@pytest.fixture
def mock_sleep() -> Generator[Mock]:
    """Mock time.sleep so delays don't slow down tests."""
    with patch("time.sleep") as mock:
        yield mock


@pytest.fixture
def mock_time(mock_sleep: Mock) -> Generator[None]:
    """Mock time-related functions for consistent testing."""
    # Monotonically increasing, never exhausted regardless of how many timing
    # windows perform_ota measures or how many times a test calls it
    with patch("time.perf_counter", side_effect=itertools.count()):
        yield


@pytest.fixture
def mock_token_hex() -> Generator[Mock]:
    """Mock secrets.token_hex for predictable test values."""

    def _token_hex(nbytes: int) -> str:
        if nbytes == 16:
            return MOCK_MD5_CNONCE
        if nbytes == 32:
            return MOCK_SHA256_CNONCE
        raise ValueError(f"Unexpected nbytes for token_hex mock: {nbytes}")

    with patch("esphome.espota2.secrets.token_hex", side_effect=_token_hex) as mock:
        yield mock


@pytest.fixture
def mock_resolve_ip() -> Generator[Mock]:
    """Mock resolve_ip_address for testing."""
    with patch("esphome.espota2.resolve_ip_address") as mock:
        mock.return_value = [
            (socket.AF_INET, socket.SOCK_STREAM, 0, "", ("192.168.1.100", 3232))
        ]
        yield mock


DUAL_STACK_SA6 = ("2001:db8::1", 3232, 0, 0)
DUAL_STACK_SA4 = ("192.168.1.100", 3232)


@pytest.fixture
def mock_resolve_ip_dual(mock_resolve_ip: Mock) -> Mock:
    """Make resolve_ip_address return an IPv6 and an IPv4 address."""
    mock_resolve_ip.return_value = [
        (socket.AF_INET6, socket.SOCK_STREAM, 0, "", DUAL_STACK_SA6),
        (socket.AF_INET, socket.SOCK_STREAM, 0, "", DUAL_STACK_SA4),
    ]
    return mock_resolve_ip


@pytest.fixture
def firmware_file(tmp_path: Path) -> Path:
    """Create a firmware file on disk for run_ota_impl_ tests."""
    firmware = tmp_path / "firmware.bin"
    firmware.write_bytes(b"firmware content")
    return firmware


@pytest.fixture
def mock_perform_ota() -> Generator[Mock]:
    """Mock perform_ota function for testing."""
    with patch("esphome.espota2.perform_ota") as mock:
        yield mock


@pytest.fixture
def mock_run_ota_impl() -> Generator[Mock]:
    """Mock run_ota_impl_ function for testing."""
    with patch("esphome.espota2.run_ota_impl_") as mock:
        mock.return_value = (0, "192.168.1.100")
        yield mock


@pytest.fixture
def mock_socket_constructor(mock_socket: Mock) -> Generator[Mock]:
    """Mock socket.socket constructor to return our mock socket."""
    with patch("socket.socket", return_value=mock_socket) as mock_constructor:
        yield mock_constructor


def test_recv_decode_with_decode(mock_socket: Mock) -> None:
    """Test recv_decode with decode=True returns list."""
    mock_socket.recv.return_value = b"\x01\x02\x03"

    result = espota2.recv_decode(mock_socket, 3, decode=True)

    assert result == [1, 2, 3]
    mock_socket.recv.assert_called_once_with(3)


def test_recv_decode_without_decode(mock_socket: Mock) -> None:
    """Test recv_decode with decode=False returns bytes."""
    mock_socket.recv.return_value = b"\x01\x02\x03"

    result = espota2.recv_decode(mock_socket, 3, decode=False)

    assert result == b"\x01\x02\x03"
    mock_socket.recv.assert_called_once_with(3)


def test_receive_exactly_success(mock_socket: Mock) -> None:
    """Test receive_exactly successfully receives expected data."""
    mock_socket.recv.side_effect = [b"\x00", b"\x01\x02"]

    result = espota2.receive_exactly(mock_socket, 3, "test", espota2.RESPONSE_OK)

    assert result == [0, 1, 2]
    assert mock_socket.recv.call_count == 2


def test_receive_exactly_with_error_response(mock_socket: Mock) -> None:
    """Test receive_exactly raises OTAError on error response."""
    mock_socket.recv.return_value = bytes([espota2.RESPONSE_ERROR_AUTH_INVALID])

    with pytest.raises(
        espota2.OTAError, match="receiving auth:.*Authentication invalid"
    ) as exc_info:
        espota2.receive_exactly(mock_socket, 1, "auth", [espota2.RESPONSE_OK])

    # Device-reported errors must stay plain OTAError, not the retryable kind
    assert not isinstance(exc_info.value, espota2.OTANetworkError)
    mock_socket.close.assert_called_once()


def test_receive_exactly_socket_error(mock_socket: Mock) -> None:
    """Test receive_exactly handles socket errors."""
    mock_socket.recv.side_effect = OSError("Connection reset")

    with pytest.raises(espota2.OTANetworkError, match="receiving test response"):
        espota2.receive_exactly(mock_socket, 1, "test", espota2.RESPONSE_OK)


def test_receive_exactly_mid_read_socket_error(mock_socket: Mock) -> None:
    """Test receive_exactly handles socket errors after the first byte."""
    mock_socket.recv.side_effect = [b"\x00", OSError("Connection reset")]

    with pytest.raises(espota2.OTANetworkError, match="receiving test:"):
        espota2.receive_exactly(mock_socket, 3, "test", espota2.RESPONSE_OK)


def test_receive_exactly_closed_connection_is_network_error(mock_socket: Mock) -> None:
    """Test receive_exactly raises OTANetworkError when the device closes the connection."""
    mock_socket.recv.return_value = b""

    with pytest.raises(
        espota2.OTANetworkError, match="Device closed connection without responding"
    ):
        espota2.receive_exactly(mock_socket, 1, "test", espota2.RESPONSE_OK)

    mock_socket.close.assert_called_once()


@pytest.mark.parametrize(
    ("error_code", "expected_msg"),
    [
        (espota2.RESPONSE_ERROR_MAGIC, "Invalid magic byte"),
        (espota2.RESPONSE_ERROR_UPDATE_PREPARE, "Couldn't prepare flash memory"),
        (espota2.RESPONSE_ERROR_AUTH_INVALID, "Authentication invalid"),
        (
            espota2.RESPONSE_ERROR_WRITING_FLASH,
            "Writing OTA data to flash memory failed",
        ),
        (espota2.RESPONSE_ERROR_UPDATE_END, "Finishing update failed"),
        (
            espota2.RESPONSE_ERROR_INVALID_BOOTSTRAPPING,
            "Please press the reset button",
        ),
        (
            espota2.RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG,
            "ESP has been flashed with wrong flash size",
        ),
        (
            espota2.RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG,
            "ESP does not have the requested flash size",
        ),
        (
            espota2.RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE,
            "ESP does not have enough space",
        ),
        (
            espota2.RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE,
            "The OTA partition on the ESP is too small",
        ),
        (
            espota2.RESPONSE_ERROR_NO_UPDATE_PARTITION,
            "The OTA partition on the ESP couldn't be found",
        ),
        (espota2.RESPONSE_ERROR_MD5_MISMATCH, "Application MD5 code mismatch"),
        (
            espota2.RESPONSE_ERROR_SIGNATURE_INVALID,
            "Firmware signature verification failed",
        ),
        (
            espota2.RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE,
            "The requested OTA type is not supported by the device",
        ),
        (
            espota2.RESPONSE_ERROR_PARTITION_TABLE_VERIFY,
            "The partition table update could not be verified",
        ),
        (
            espota2.RESPONSE_ERROR_PARTITION_TABLE_UPDATE,
            "An error occurred while updating the partition table",
        ),
        (
            espota2.RESPONSE_ERROR_BOOTLOADER_VERIFY,
            "The bootloader update could not be verified",
        ),
        (
            espota2.RESPONSE_ERROR_BOOTLOADER_UPDATE,
            "An error occurred while updating the bootloader",
        ),
        (espota2.RESPONSE_ERROR_UNKNOWN, "Unknown error from ESP"),
    ],
)
def test_check_error_with_various_errors(error_code: int, expected_msg: str) -> None:
    """Test check_error raises appropriate errors for different error codes."""
    with pytest.raises(espota2.OTAError, match=expected_msg):
        espota2.check_error([error_code], [espota2.RESPONSE_OK])


def test_check_error_unexpected_response() -> None:
    """Test check_error raises error for unexpected response."""
    with pytest.raises(espota2.OTAError, match="Unexpected response from ESP: 0x7F"):
        espota2.check_error([0x7F], [espota2.RESPONSE_OK, espota2.RESPONSE_AUTH_OK])


def test_check_error_empty_data() -> None:
    """Test check_error raises the retryable OTANetworkError when the device closes the connection."""
    with pytest.raises(
        espota2.OTANetworkError, match="Device closed connection without responding"
    ):
        espota2.check_error([], [espota2.RESPONSE_OK])

    # Also test with empty bytes
    with pytest.raises(
        espota2.OTANetworkError, match="Device closed connection without responding"
    ):
        espota2.check_error(b"", [espota2.RESPONSE_OK])


def test_send_check_with_various_data_types(mock_socket: Mock) -> None:
    """Test send_check handles different data types."""

    # Test with list/tuple
    espota2.send_check(mock_socket, [0x01, 0x02], "list")
    mock_socket.sendall.assert_called_with(b"\x01\x02")

    # Test with int
    espota2.send_check(mock_socket, 0x42, "int")
    mock_socket.sendall.assert_called_with(b"\x42")

    # Test with string
    espota2.send_check(mock_socket, "hello", "string")
    mock_socket.sendall.assert_called_with(b"hello")

    # Test with bytes (should pass through)
    espota2.send_check(mock_socket, b"\xaa\xbb", "bytes")
    mock_socket.sendall.assert_called_with(b"\xaa\xbb")


def test_send_check_socket_error(mock_socket: Mock) -> None:
    """Test send_check handles socket errors."""
    mock_socket.sendall.side_effect = OSError("Broken pipe")

    with pytest.raises(espota2.OTAError, match="sending test"):
        espota2.send_check(mock_socket, b"data", "test")


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_successful_md5_auth(
    mock_socket: Mock, mock_file: io.BytesIO, mock_token_hex: Mock
) -> None:
    """Test successful OTA with MD5 authentication."""
    # Setup socket responses for recv calls
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_HEADER_OK]),  # Features response
        bytes([espota2.RESPONSE_REQUEST_AUTH]),  # Auth request
        MOCK_MD5_NONCE,  # 32 char hex nonce
        bytes([espota2.RESPONSE_AUTH_OK]),  # Auth result
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),  # Binary size OK
        bytes([espota2.RESPONSE_BIN_MD5_OK]),  # MD5 checksum OK
        bytes([espota2.RESPONSE_CHUNK_OK]),  # Chunk OK
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Receive OK
        bytes([espota2.RESPONSE_UPDATE_END_OK]),  # Update end OK
    ]

    mock_socket.recv.side_effect = recv_responses

    # Run OTA
    espota2.perform_ota(mock_socket, "testpass", mock_file, "test.bin")

    # Verify magic bytes were sent
    assert mock_socket.sendall.call_args_list[0] == call(bytes(espota2.MAGIC_BYTES))

    # Verify features were sent (compression + SHA256 support + extended protocol)
    assert mock_socket.sendall.call_args_list[1] == call(
        bytes(
            [
                espota2.CLIENT_FEATURE_SUPPORTS_COMPRESSION
                | espota2.CLIENT_FEATURE_SUPPORTS_SHA256_AUTH
                | espota2.CLIENT_FEATURE_SUPPORTS_EXTENDED_PROTOCOL
                | espota2.CLIENT_FEATURE_SUPPORTS_DEFLATE
                | espota2.CLIENT_FEATURE_SUPPORTS_UDP
            ]
        )
    )

    # Verify token_hex was called with MD5 digest size
    mock_token_hex.assert_called_once_with(16)

    # Verify cnonce was sent
    cnonce = MOCK_MD5_CNONCE
    assert mock_socket.sendall.call_args_list[2] == call(cnonce.encode())

    # Verify auth result was computed correctly
    expected_hash = hashlib.md5()
    expected_hash.update(b"testpass")
    expected_hash.update(MOCK_MD5_NONCE)
    expected_hash.update(cnonce.encode())
    expected_result = expected_hash.hexdigest()
    assert mock_socket.sendall.call_args_list[3] == call(expected_result.encode())


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_no_auth(
    mock_socket: Mock, mock_file: io.BytesIO, caplog: pytest.LogCaptureFixture
) -> None:
    """Test OTA without authentication."""
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_1_0]),  # Version number
        bytes([espota2.RESPONSE_HEADER_OK]),  # Features response
        bytes([espota2.RESPONSE_AUTH_OK]),  # No auth required
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),  # Binary size OK
        bytes([espota2.RESPONSE_BIN_MD5_OK]),  # MD5 checksum OK
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Receive OK
        bytes([espota2.RESPONSE_UPDATE_END_OK]),  # Update end OK
    ]

    mock_socket.recv.side_effect = recv_responses

    # Distinct window lengths pin each duration to its label; exactly the 6
    # expected perf_counter calls, so an unaccounted timing window raises
    timings = [0.0, 2.0, 10.0, 15.0, 20.0, 27.0]
    with (
        patch("time.perf_counter", side_effect=timings),
        caplog.at_level(logging.INFO),
    ):
        espota2.perform_ota(mock_socket, None, mock_file, "test.bin")

    # Should not send any auth-related data
    auth_calls = [
        call
        for call in mock_socket.sendall.call_args_list
        if "cnonce" in str(call) or "result" in str(call)
    ]
    assert len(auth_calls) == 0

    # The timing summary is the observable output of the upload; exact strings
    # pin each duration to its label
    assert "Preparing for upload took 2.00 seconds" in caplog.text
    assert (
        "Update took 14.00 seconds (prepare 2.00, upload 5.00, commit 7.00)"
        in caplog.text
    )
    # The data phase timeout must outlast the device's 105 s data timeout
    mock_socket.settimeout.assert_any_call(espota2.DATA_PHASE_TIMEOUT)
    assert espota2.DATA_PHASE_TIMEOUT > 105.0


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_with_compression(mock_socket: Mock) -> None:
    """Test OTA with compression support."""
    original_content = b"firmware" * 100  # Repeating content for compression
    mock_file = io.BytesIO(original_content)
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_SUPPORTS_COMPRESSION]),  # Device supports compression
        bytes([espota2.RESPONSE_AUTH_OK]),  # No auth required
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),  # Binary size OK
        bytes([espota2.RESPONSE_BIN_MD5_OK]),  # MD5 checksum OK
        bytes([espota2.RESPONSE_CHUNK_OK]),  # Chunk OK
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Receive OK
        bytes([espota2.RESPONSE_UPDATE_END_OK]),  # Update end OK
    ]

    mock_socket.recv.side_effect = recv_responses

    espota2.perform_ota(mock_socket, None, mock_file, "test.bin")

    # Verify compressed content was sent
    # Get the binary size that was sent (4 bytes after features)
    size_bytes = mock_socket.sendall.call_args_list[2][0][0]
    sent_size = struct.unpack(">I", size_bytes)[0]

    # Size should be less than original due to compression
    assert sent_size < len(original_content)

    # Verify the content sent was gzipped
    compressed = gzip.compress(original_content, compresslevel=9)
    assert sent_size == len(compressed)


def test_perform_ota_auth_without_password(mock_socket: Mock) -> None:
    """Test OTA fails when auth is required but no password provided."""
    mock_file = io.BytesIO(b"firmware")

    responses = [
        bytes([espota2.RESPONSE_OK, espota2.OTA_VERSION_2_0]),
        bytes([espota2.RESPONSE_HEADER_OK]),
        bytes([espota2.RESPONSE_REQUEST_AUTH]),
    ]

    mock_socket.recv.side_effect = responses

    with pytest.raises(
        espota2.OTAError, match="ESP requests password, but no password given"
    ):
        espota2.perform_ota(mock_socket, None, mock_file, "test.bin")


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_md5_auth_wrong_password(
    mock_socket: Mock, mock_file: io.BytesIO, mock_token_hex: Mock
) -> None:
    """Test OTA fails when MD5 authentication is rejected due to wrong password."""
    # Setup socket responses for recv calls
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_HEADER_OK]),  # Features response
        bytes([espota2.RESPONSE_REQUEST_AUTH]),  # Auth request
        MOCK_MD5_NONCE,  # 32 char hex nonce
        bytes([espota2.RESPONSE_ERROR_AUTH_INVALID]),  # Auth rejected!
    ]

    mock_socket.recv.side_effect = recv_responses

    with pytest.raises(
        espota2.OTAError, match="receiving auth.*Authentication invalid"
    ):
        espota2.perform_ota(mock_socket, "wrongpassword", mock_file, "test.bin")

    # Verify the socket was closed after auth failure
    mock_socket.close.assert_called()


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_sha256_auth_wrong_password(
    mock_socket: Mock, mock_file: io.BytesIO, mock_token_hex: Mock
) -> None:
    """Test OTA fails when SHA256 authentication is rejected due to wrong password."""
    # Setup socket responses for recv calls
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_HEADER_OK]),  # Features response
        bytes([espota2.RESPONSE_REQUEST_SHA256_AUTH]),  # SHA256 Auth request
        MOCK_SHA256_NONCE,  # 64 char hex nonce
        bytes([espota2.RESPONSE_ERROR_AUTH_INVALID]),  # Auth rejected!
    ]

    mock_socket.recv.side_effect = recv_responses

    with pytest.raises(
        espota2.OTAError, match="receiving auth.*Authentication invalid"
    ):
        espota2.perform_ota(mock_socket, "wrongpassword", mock_file, "test.bin")

    # Verify the socket was closed after auth failure
    mock_socket.close.assert_called()


def test_perform_ota_sha256_auth_without_password(mock_socket: Mock) -> None:
    """Test OTA fails when SHA256 auth is required but no password provided."""
    mock_file = io.BytesIO(b"firmware")

    responses = [
        bytes([espota2.RESPONSE_OK, espota2.OTA_VERSION_2_0]),
        bytes([espota2.RESPONSE_HEADER_OK]),
        bytes([espota2.RESPONSE_REQUEST_SHA256_AUTH]),
    ]

    mock_socket.recv.side_effect = responses

    with pytest.raises(
        espota2.OTAError, match="ESP requests password, but no password given"
    ):
        espota2.perform_ota(mock_socket, None, mock_file, "test.bin")


def test_perform_ota_unexpected_auth_response(mock_socket: Mock) -> None:
    """Test OTA fails when device sends an unexpected auth response."""
    mock_file = io.BytesIO(b"firmware")

    # Use 0x03 which is not in the expected auth responses
    # This will be caught by check_error and raise "Unexpected response from ESP"
    UNKNOWN_AUTH_METHOD = 0x03

    responses = [
        bytes([espota2.RESPONSE_OK, espota2.OTA_VERSION_2_0]),
        bytes([espota2.RESPONSE_HEADER_OK]),
        bytes([UNKNOWN_AUTH_METHOD]),  # Unknown auth method
    ]

    mock_socket.recv.side_effect = responses

    # This will actually raise "Unexpected response from ESP" from check_error
    with pytest.raises(
        espota2.OTAError, match=r"receiving auth: Unexpected response from ESP: 0x03"
    ):
        espota2.perform_ota(mock_socket, "password", mock_file, "test.bin")


def test_perform_ota_unsupported_version(mock_socket: Mock) -> None:
    """Test OTA fails with unsupported version."""
    mock_file = io.BytesIO(b"firmware")

    responses = [
        bytes([espota2.RESPONSE_OK, 99]),  # Unsupported version
    ]

    mock_socket.recv.side_effect = responses

    with pytest.raises(espota2.OTAError, match="Device uses unsupported OTA version"):
        espota2.perform_ota(mock_socket, None, mock_file, "test.bin")


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_upload_error(mock_socket: Mock, mock_file: io.BytesIO) -> None:
    """Test OTA handles upload errors."""
    # Setup responses - provide enough for the recv calls
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_HEADER_OK]),  # Features response
        bytes([espota2.RESPONSE_AUTH_OK]),  # No auth required
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),  # Binary size OK
        bytes([espota2.RESPONSE_BIN_MD5_OK]),  # MD5 checksum OK
    ]
    # Add OSError to recv to simulate connection loss during chunk read
    recv_responses.append(OSError("Connection lost"))

    mock_socket.recv.side_effect = recv_responses

    with pytest.raises(espota2.OTAError, match="receiving chunk result response"):
        espota2.perform_ota(mock_socket, None, mock_file, "test.bin")


def _no_auth_handshake(version: int, server_features: int | None = None) -> list[bytes]:
    """Recv responses for a handshake without auth, up to the MD5 check."""
    if server_features is None:
        features = [bytes([espota2.RESPONSE_HEADER_OK])]
    else:
        features = [bytes([espota2.RESPONSE_FEATURE_FLAGS]), bytes([server_features])]
    return [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([version]),  # Version number
        *features,
        bytes([espota2.RESPONSE_AUTH_OK]),  # No auth required
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),  # Binary size OK
        bytes([espota2.RESPONSE_BIN_MD5_OK]),  # MD5 checksum OK
    ]


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_chunk_send_error(mock_socket: Mock, mock_file: io.BytesIO) -> None:
    """Test OTA raises the retryable OTANetworkError when sending a chunk fails."""
    mock_socket.recv.side_effect = [
        *_no_auth_handshake(espota2.OTA_VERSION_2_0),
        OSError("Connection reset"),  # Probe for a pending error byte fails too
    ]
    # Sends before the data phase: magic bytes, features, binary size, MD5;
    # fail on the fifth sendall, the first firmware chunk
    mock_socket.sendall.side_effect = [None] * 4 + [OSError("Broken pipe")]

    with pytest.raises(espota2.OTANetworkError, match="sending data:"):
        espota2.perform_ota(mock_socket, None, mock_file, "test.bin")


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_chunk_send_error_surfaces_device_error(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """Test a device error byte pending behind a send failure becomes the cause."""
    mock_socket.recv.side_effect = [
        *_no_auth_handshake(espota2.OTA_VERSION_1_0),
        bytes([espota2.RESPONSE_ERROR_WRITING_FLASH]),  # Reason the device closed
    ]
    mock_socket.sendall.side_effect = [None] * 4 + [OSError("Broken pipe")]

    with pytest.raises(
        espota2.OTAError, match="Writing OTA data to flash memory failed"
    ) as exc:
        espota2.perform_ota(mock_socket, None, mock_file, "test.bin")

    # The device-reported error is not retryable
    assert not isinstance(exc.value, espota2.OTANetworkError)


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_final_chunk_ack_failure_not_retryable(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """Test a lost ack for the final chunk is not retried."""
    mock_socket.recv.side_effect = [
        *_no_auth_handshake(espota2.OTA_VERSION_2_0),
        OSError("Connection reset"),  # Ack for the only (final) chunk is lost
    ]

    with pytest.raises(espota2.OTAError, match="receiving chunk result") as exc:
        espota2.perform_ota(mock_socket, None, mock_file, "test.bin")

    # The device already had the whole image, so it may be committing
    assert not isinstance(exc.value, espota2.OTANetworkError)


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_intermediate_chunk_ack_failure_retryable(
    mock_socket: Mock,
) -> None:
    """Test a lost ack for a non-final chunk stays retryable."""
    # Two chunks: the firmware is larger than one upload block
    big_file = io.BytesIO(b"x" * (espota2.UPLOAD_BLOCK_SIZE + 1))
    mock_socket.recv.side_effect = [
        *_no_auth_handshake(espota2.OTA_VERSION_2_0),
        OSError("Connection reset"),  # Ack for the first of two chunks is lost
    ]

    with pytest.raises(espota2.OTANetworkError, match="receiving chunk result"):
        espota2.perform_ota(mock_socket, None, big_file, "test.bin")


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_post_commit_failure_not_retryable(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """Test a network failure after the device committed is a plain OTAError."""
    mock_socket.recv.side_effect = [
        *_no_auth_handshake(espota2.OTA_VERSION_1_0),
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Device received everything
        OSError("Connection reset"),  # Connection lost waiting for end result
    ]

    with pytest.raises(espota2.OTAError, match="receiving update end result") as exc:
        espota2.perform_ota(mock_socket, None, mock_file, "test.bin")

    # Must not be the retryable kind; the device is already rebooting
    assert not isinstance(exc.value, espota2.OTANetworkError)


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_md5_mismatch_not_marked_committed(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """Test an MD5 mismatch keeps its own message and stays non-retryable."""
    mock_socket.recv.side_effect = [
        *_no_auth_handshake(espota2.OTA_VERSION_1_0),
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Device received everything
        bytes([espota2.RESPONSE_ERROR_MD5_MISMATCH]),  # Device aborted the update
    ]

    with pytest.raises(espota2.OTAError, match="MD5 code mismatch") as exc:
        espota2.perform_ota(mock_socket, None, mock_file, "test.bin")

    # The device aborted without committing, so the message must not claim
    # the update may have been installed, and the error must not be retried
    assert not isinstance(exc.value, espota2.OTANetworkError)
    assert "committed" not in str(exc.value)


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_end_ack_send_failure_is_success(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """Test a send failure on the final acknowledgement does not fail the OTA."""
    mock_socket.recv.side_effect = [
        *_no_auth_handshake(espota2.OTA_VERSION_1_0),
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Device received everything
        bytes([espota2.RESPONSE_UPDATE_END_OK]),  # Update committed
    ]
    # Sends: magic bytes, features, binary size, MD5, one firmware chunk;
    # fail on the sixth sendall, the end acknowledgement
    mock_socket.sendall.side_effect = [None] * 5 + [OSError("Broken pipe")]

    # Must not raise; the device treats a missing acknowledgement as non-fatal
    espota2.perform_ota(mock_socket, None, mock_file, "test.bin")

    assert mock_socket.sendall.call_count == 6


@pytest.mark.usefixtures("mock_socket_constructor", "mock_resolve_ip")
def test_run_ota_impl_successful(
    mock_socket: Mock, tmp_path: Path, mock_perform_ota: Mock
) -> None:
    """Test run_ota_impl_ with successful upload."""
    # Create a real firmware file
    firmware_file = tmp_path / "firmware.bin"
    firmware_file.write_bytes(b"firmware content")

    # Run OTA with real file path
    result_code, result_host = espota2.run_ota_impl_(
        "test.local", 3232, "password", str(firmware_file)
    )

    # Verify success
    assert result_code == 0
    assert result_host == "192.168.1.100"

    # Verify socket was configured correctly
    mock_socket.settimeout.assert_called_with(espota2.SETUP_TIMEOUT)
    mock_socket.connect.assert_called_once_with(("192.168.1.100", 3232))
    mock_socket.close.assert_called_once()

    # Verify perform_ota was called with real file
    mock_perform_ota.assert_called_once()
    call_args = mock_perform_ota.call_args[0]
    assert call_args[0] == mock_socket
    assert call_args[1] == "password"
    # Verify the file object is a proper file handle
    assert isinstance(call_args[2], io.IOBase)
    assert call_args[3] == str(firmware_file)


@pytest.mark.usefixtures("mock_socket_constructor", "mock_resolve_ip")
def test_run_ota_impl_connection_failed(
    mock_socket: Mock, firmware_file: Path, mock_sleep: Mock
) -> None:
    """Test run_ota_impl_ retries when connection fails and eventually gives up."""
    mock_socket.connect.side_effect = OSError("Connection refused")

    result_code, result_host = espota2.run_ota_impl_(
        "test.local", 3232, "password", str(firmware_file)
    )

    assert result_code == 1
    assert result_host is None
    # A single address gets the whole attempt budget, with a delay before
    # each revisit
    assert mock_socket.connect.call_count == espota2.EXTRA_UPLOAD_ATTEMPTS + 1
    assert mock_socket.close.call_count == espota2.EXTRA_UPLOAD_ATTEMPTS + 1
    assert mock_sleep.call_count == espota2.EXTRA_UPLOAD_ATTEMPTS
    mock_sleep.assert_called_with(espota2.UPLOAD_RETRY_DELAY)


@pytest.mark.usefixtures("mock_socket_constructor", "mock_resolve_ip")
def test_run_ota_impl_connect_retry_succeeds(
    mock_socket: Mock, firmware_file: Path, mock_perform_ota: Mock, mock_sleep: Mock
) -> None:
    """Test run_ota_impl_ succeeds when a retry connects after a failed attempt."""
    mock_socket.connect.side_effect = [OSError("Connection timed out"), None]

    result_code, result_host = espota2.run_ota_impl_(
        "test.local", 3232, "password", str(firmware_file)
    )

    assert result_code == 0
    assert result_host == "192.168.1.100"
    assert mock_socket.connect.call_count == 2
    mock_sleep.assert_called_once_with(espota2.UPLOAD_RETRY_DELAY)
    mock_perform_ota.assert_called_once()


@pytest.mark.usefixtures("mock_socket_constructor", "mock_resolve_ip")
def test_run_ota_impl_network_error_retry_succeeds(
    mock_socket: Mock, firmware_file: Path, mock_perform_ota: Mock, mock_sleep: Mock
) -> None:
    """Test run_ota_impl_ retries after a network error during the upload."""
    mock_perform_ota.side_effect = [
        espota2.OTANetworkError("receiving features: Device closed connection"),
        None,
    ]

    result_code, result_host = espota2.run_ota_impl_(
        "test.local", 3232, "password", str(firmware_file)
    )

    assert result_code == 0
    assert result_host == "192.168.1.100"
    assert mock_perform_ota.call_count == 2
    mock_sleep.assert_called_once_with(espota2.UPLOAD_RETRY_DELAY)
    # The failure marks the link lossy, so the retry moves the data to UDP
    assert [c.kwargs["prefer_udp"] for c in mock_perform_ota.call_args_list] == [
        False,
        True,
    ]


@pytest.mark.usefixtures("mock_socket_constructor", "mock_resolve_ip")
def test_run_ota_impl_network_error_exhausts_attempts(
    mock_socket: Mock, firmware_file: Path, mock_perform_ota: Mock, mock_sleep: Mock
) -> None:
    """Test run_ota_impl_ gives up after all attempts hit network errors.

    Every attempt connected, so the budget is the longer one for a reachable
    device on a lossy link.
    """
    mock_perform_ota.side_effect = espota2.OTANetworkError("sending data: broken pipe")

    result_code, result_host = espota2.run_ota_impl_(
        "test.local", 3232, "password", str(firmware_file)
    )

    assert result_code == 1
    assert result_host is None
    assert mock_perform_ota.call_count == espota2.EXTRA_UPLOAD_ATTEMPTS_REACHED + 1
    assert mock_sleep.call_count == espota2.EXTRA_UPLOAD_ATTEMPTS_REACHED


@pytest.mark.usefixtures("mock_socket_constructor", "mock_resolve_ip_dual")
def test_run_ota_impl_multiple_addresses_cycle(
    mock_socket: Mock, firmware_file: Path, mock_sleep: Mock
) -> None:
    """Test run_ota_impl_ visits every address and cycles for the retries."""
    mock_socket.connect.side_effect = OSError("No route to host")

    result_code, result_host = espota2.run_ota_impl_(
        "test.local", 3232, "password", str(firmware_file)
    )

    assert result_code == 1
    assert result_host is None
    # Each address is visited once, then the EXTRA_UPLOAD_ATTEMPTS spare
    # attempts cycle back through them; the budget is shared, not per address
    assert mock_socket.connect.call_args_list == [
        call(DUAL_STACK_SA6),
        call(DUAL_STACK_SA4),
    ] * (1 + espota2.EXTRA_UPLOAD_ATTEMPTS // 2)
    # No connect ever reached the device, so the delay only applies before
    # the revisits
    assert mock_sleep.call_count == espota2.EXTRA_UPLOAD_ATTEMPTS


@pytest.mark.usefixtures("mock_socket_constructor", "mock_resolve_ip_dual")
def test_run_ota_impl_second_address_succeeds_without_delay(
    mock_socket: Mock,
    firmware_file: Path,
    mock_perform_ota: Mock,
    mock_sleep: Mock,
) -> None:
    """Test run_ota_impl_ falls through to the next address with no pause."""
    mock_socket.connect.side_effect = [OSError("No route to host"), None]

    result_code, result_host = espota2.run_ota_impl_(
        "test.local", 3232, "password", str(firmware_file)
    )

    assert result_code == 0
    assert result_host == "192.168.1.100"
    mock_sleep.assert_not_called()
    mock_perform_ota.assert_called_once()


@pytest.mark.usefixtures("mock_socket_constructor", "mock_resolve_ip_dual")
def test_run_ota_impl_pauses_after_reaching_device(
    mock_socket: Mock,
    firmware_file: Path,
    mock_perform_ota: Mock,
    mock_sleep: Mock,
) -> None:
    """Test run_ota_impl_ pauses before the next address once the device was reached."""
    mock_perform_ota.side_effect = [
        espota2.OTANetworkError("sending data: connection reset"),
        None,
    ]

    result_code, result_host = espota2.run_ota_impl_(
        "test.local", 3232, "password", str(firmware_file)
    )

    assert result_code == 0
    assert result_host == "192.168.1.100"
    # The first attempt reached the device, so the next one waits first even
    # though it targets a fresh address
    mock_sleep.assert_called_once_with(espota2.UPLOAD_RETRY_DELAY)


@pytest.mark.usefixtures("mock_socket_constructor", "mock_resolve_ip")
def test_run_ota_impl_device_error_not_retried(
    mock_socket: Mock, firmware_file: Path, mock_perform_ota: Mock, mock_sleep: Mock
) -> None:
    """Test run_ota_impl_ fails immediately on a device-reported error."""
    mock_perform_ota.side_effect = espota2.OTAError(
        "Authentication invalid. Is the password correct?"
    )

    result_code, result_host = espota2.run_ota_impl_(
        "test.local", 3232, "password", str(firmware_file)
    )

    assert result_code == 1
    assert result_host is None
    mock_perform_ota.assert_called_once()
    mock_sleep.assert_not_called()


def test_run_ota_impl_no_addresses(
    firmware_file: Path, mock_resolve_ip: Mock, mock_sleep: Mock
) -> None:
    """Test run_ota_impl_ fails cleanly when resolution yields no addresses."""
    mock_resolve_ip.return_value = []

    result_code, result_host = espota2.run_ota_impl_(
        "test.local", 3232, "password", str(firmware_file)
    )

    assert result_code == 1
    assert result_host is None
    mock_sleep.assert_not_called()


def test_run_ota_impl_resolve_failed(tmp_path: Path, mock_resolve_ip: Mock) -> None:
    """Test run_ota_impl_ when DNS resolution fails."""
    # Create a real firmware file
    firmware_file = tmp_path / "firmware.bin"
    firmware_file.write_bytes(b"firmware content")

    mock_resolve_ip.side_effect = EsphomeError("DNS resolution failed")

    with pytest.raises(espota2.OTAError, match="DNS resolution failed"):
        result_code, result_host = espota2.run_ota_impl_(
            "unknown.host", 3232, "password", str(firmware_file)
        )


def test_run_ota_wrapper(mock_run_ota_impl: Mock) -> None:
    """Test run_ota wrapper function."""
    # Test successful case
    mock_run_ota_impl.return_value = (0, "192.168.1.100")
    result = espota2.run_ota("test.local", 3232, "pass", "fw.bin")
    assert result == (0, "192.168.1.100")

    # Test error case
    mock_run_ota_impl.side_effect = espota2.OTAError("Test error")
    result = espota2.run_ota("test.local", 3232, "pass", "fw.bin")
    assert result == (1, None)


def test_progress_bar(capsys: CaptureFixture[str]) -> None:
    """Test ProgressBar functionality."""
    progress = espota2.ProgressBar("Uploading")
    progress.enabled = True  # Fake TTY

    # Test initial update
    progress.update(0.0)
    captured = capsys.readouterr()
    assert "0%" in captured.err
    assert "[" in captured.err

    # Test progress update
    progress.update(0.5)
    captured = capsys.readouterr()
    assert "50%" in captured.err

    # Test completion
    progress.update(1.0)
    captured = capsys.readouterr()
    assert "100%" in captured.err
    assert "Done" in captured.err

    # done() after the 100% frame adds nothing; that frame ended its line
    progress.done()
    captured = capsys.readouterr()
    assert captured.err == ""

    # Test same progress doesn't update
    progress.update(0.5)
    progress.update(0.5)
    captured = capsys.readouterr()
    # Should only see one update (second call shouldn't write)
    assert captured.err.count("50%") == 1

    # done() after a mid-way frame ends the line
    progress.done()
    assert capsys.readouterr().err == "\n"


# Tests for SHA256 authentication
@pytest.mark.usefixtures("mock_time")
def test_perform_ota_successful_sha256_auth(
    mock_socket: Mock, mock_file: io.BytesIO, mock_token_hex: Mock
) -> None:
    """Test successful OTA with SHA256 authentication."""
    # Setup socket responses for recv calls
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_HEADER_OK]),  # Features response
        bytes([espota2.RESPONSE_REQUEST_SHA256_AUTH]),  # SHA256 Auth request
        MOCK_SHA256_NONCE,  # 64 char hex nonce
        bytes([espota2.RESPONSE_AUTH_OK]),  # Auth result
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),  # Binary size OK
        bytes([espota2.RESPONSE_BIN_MD5_OK]),  # MD5 checksum OK
        bytes([espota2.RESPONSE_CHUNK_OK]),  # Chunk OK
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Receive OK
        bytes([espota2.RESPONSE_UPDATE_END_OK]),  # Update end OK
    ]

    mock_socket.recv.side_effect = recv_responses

    # Run OTA
    espota2.perform_ota(mock_socket, "testpass", mock_file, "test.bin")

    # Verify magic bytes were sent
    assert mock_socket.sendall.call_args_list[0] == call(bytes(espota2.MAGIC_BYTES))

    # Verify features were sent (compression + SHA256 support + extended protocol)
    assert mock_socket.sendall.call_args_list[1] == call(
        bytes(
            [
                espota2.CLIENT_FEATURE_SUPPORTS_COMPRESSION
                | espota2.CLIENT_FEATURE_SUPPORTS_SHA256_AUTH
                | espota2.CLIENT_FEATURE_SUPPORTS_EXTENDED_PROTOCOL
                | espota2.CLIENT_FEATURE_SUPPORTS_DEFLATE
                | espota2.CLIENT_FEATURE_SUPPORTS_UDP
            ]
        )
    )

    # Verify token_hex was called with SHA256 digest size
    mock_token_hex.assert_called_once_with(32)

    # Verify cnonce was sent
    cnonce = MOCK_SHA256_CNONCE
    assert mock_socket.sendall.call_args_list[2] == call(cnonce.encode())

    # Verify auth result was computed correctly with SHA256
    expected_hash = hashlib.sha256()
    expected_hash.update(b"testpass")
    expected_hash.update(MOCK_SHA256_NONCE)
    expected_hash.update(cnonce.encode())
    expected_result = expected_hash.hexdigest()
    assert mock_socket.sendall.call_args_list[3] == call(expected_result.encode())


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_sha256_fallback_to_md5(
    mock_socket: Mock, mock_file: io.BytesIO, mock_token_hex: Mock
) -> None:
    """Test SHA256-capable client falls back to MD5 for compatibility."""
    # This test verifies the temporary backward compatibility
    # where a SHA256-capable client can still authenticate with MD5
    # This compatibility will be removed in 2026.1.0
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_HEADER_OK]),  # Features response
        bytes(
            [espota2.RESPONSE_REQUEST_AUTH]
        ),  # MD5 Auth request (device doesn't support SHA256)
        MOCK_MD5_NONCE,  # 32 char hex nonce for MD5
        bytes([espota2.RESPONSE_AUTH_OK]),  # Auth result
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),  # Binary size OK
        bytes([espota2.RESPONSE_BIN_MD5_OK]),  # MD5 checksum OK
        bytes([espota2.RESPONSE_CHUNK_OK]),  # Chunk OK
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Receive OK
        bytes([espota2.RESPONSE_UPDATE_END_OK]),  # Update end OK
    ]

    mock_socket.recv.side_effect = recv_responses

    # Run OTA - should work even though device requested MD5
    espota2.perform_ota(mock_socket, "testpass", mock_file, "test.bin")

    # Verify client still advertised SHA256 support
    assert mock_socket.sendall.call_args_list[1] == call(
        bytes(
            [
                espota2.CLIENT_FEATURE_SUPPORTS_COMPRESSION
                | espota2.CLIENT_FEATURE_SUPPORTS_SHA256_AUTH
                | espota2.CLIENT_FEATURE_SUPPORTS_EXTENDED_PROTOCOL
                | espota2.CLIENT_FEATURE_SUPPORTS_DEFLATE
                | espota2.CLIENT_FEATURE_SUPPORTS_UDP
            ]
        )
    )

    # But authentication was done with MD5
    mock_token_hex.assert_called_once_with(16)
    cnonce = MOCK_MD5_CNONCE
    expected_hash = hashlib.md5()
    expected_hash.update(b"testpass")
    expected_hash.update(MOCK_MD5_NONCE)
    expected_hash.update(cnonce.encode())
    expected_result = expected_hash.hexdigest()
    assert mock_socket.sendall.call_args_list[3] == call(expected_result.encode())


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_version_differences(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """Test OTA behavior differences between version 1.0 and 2.0."""
    # Test version 1.0 - no chunk acknowledgments
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_1_0]),  # Version number
        bytes([espota2.RESPONSE_HEADER_OK]),  # Features response
        bytes([espota2.RESPONSE_AUTH_OK]),  # No auth required
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),  # Binary size OK
        bytes([espota2.RESPONSE_BIN_MD5_OK]),  # MD5 checksum OK
        # No RESPONSE_CHUNK_OK for v1
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Receive OK
        bytes([espota2.RESPONSE_UPDATE_END_OK]),  # Update end OK
    ]

    mock_socket.recv.side_effect = recv_responses
    espota2.perform_ota(mock_socket, None, mock_file, "test.bin")

    # For v1.0, verify that we only get the expected number of recv calls
    # v1.0 doesn't have chunk acknowledgments, so fewer recv calls
    assert mock_socket.recv.call_count == 8  # v1.0 has 8 recv calls

    # Reset mock for v2.0 test
    mock_socket.reset_mock()

    # Reset file position for second test
    mock_file.seek(0)

    # Test version 2.0 - with chunk acknowledgments
    recv_responses_v2 = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_HEADER_OK]),  # Features response
        bytes([espota2.RESPONSE_AUTH_OK]),  # No auth required
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),  # Binary size OK
        bytes([espota2.RESPONSE_BIN_MD5_OK]),  # MD5 checksum OK
        bytes([espota2.RESPONSE_CHUNK_OK]),  # v2.0 has chunk acknowledgment
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Receive OK
        bytes([espota2.RESPONSE_UPDATE_END_OK]),  # Update end OK
    ]

    mock_socket.recv.side_effect = recv_responses_v2
    espota2.perform_ota(mock_socket, None, mock_file, "test.bin")

    # For v2.0, verify more recv calls due to chunk acknowledgments
    assert mock_socket.recv.call_count == 9  # v2.0 has 9 recv calls (includes chunk OK)


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_extended_protocol_app(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """Test OTA extended protocol app update."""
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_FEATURE_FLAGS]),  # Device supports extended protocol
        bytes(
            [
                espota2.SERVER_FEATURE_SUPPORTS_COMPRESSION
                | espota2.SERVER_FEATURE_SUPPORTS_PARTITION_ACCESS
            ]
        ),  # Device feature flags
        bytes([espota2.RESPONSE_AUTH_OK]),  # No auth required
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),  # Binary size OK
        bytes([espota2.RESPONSE_BIN_MD5_OK]),  # MD5 checksum OK
        bytes([espota2.RESPONSE_CHUNK_OK]),  # Chunk OK
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Receive OK
        bytes([espota2.RESPONSE_UPDATE_END_OK]),  # Update end OK
    ]

    mock_socket.recv.side_effect = recv_responses

    espota2.perform_ota(
        mock_socket,
        "testpass",
        mock_file,
        "test.bin",
        espota2.OTA_TYPE_UPDATE_APP,
    )

    # Verify magic bytes were sent
    assert mock_socket.sendall.call_args_list[0] == call(bytes(espota2.MAGIC_BYTES))

    # Verify features were sent (compression + SHA256 support + extended protocol)
    assert mock_socket.sendall.call_args_list[1] == call(
        bytes(
            [
                espota2.CLIENT_FEATURE_SUPPORTS_COMPRESSION
                | espota2.CLIENT_FEATURE_SUPPORTS_SHA256_AUTH
                | espota2.CLIENT_FEATURE_SUPPORTS_EXTENDED_PROTOCOL
                | espota2.CLIENT_FEATURE_SUPPORTS_DEFLATE
                | espota2.CLIENT_FEATURE_SUPPORTS_UDP
            ]
        )
    )

    # Verify ota type was sent
    assert mock_socket.sendall.call_args_list[2] == call(
        bytes([espota2.OTA_TYPE_UPDATE_APP])
    )


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_successful_partition_table(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """Test OTA partition table update.

    The mocked server advertises both COMPRESSION and PARTITION_ACCESS to exercise
    the full extended-protocol negotiation path. Real IDFOTABackend devices return
    ``supports_compression() == false`` and never set the COMPRESSION flag for a
    partition-table OTA; the flag here is intentional protocol-coverage, not a
    description of on-device behaviour.
    """
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_FEATURE_FLAGS]),  # Device supports extended protocol
        bytes(
            [
                espota2.SERVER_FEATURE_SUPPORTS_COMPRESSION
                | espota2.SERVER_FEATURE_SUPPORTS_PARTITION_ACCESS
            ]
        ),  # Device feature flags (compression flag is unrealistic; see docstring)
        bytes([espota2.RESPONSE_AUTH_OK]),  # No auth required
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),  # Binary size OK
        bytes([espota2.RESPONSE_BIN_MD5_OK]),  # MD5 checksum OK
        bytes([espota2.RESPONSE_CHUNK_OK]),  # Chunk OK
        bytes([espota2.RESPONSE_RECEIVE_OK]),  # Receive OK
        bytes([espota2.RESPONSE_UPDATE_END_OK]),  # Update end OK
    ]

    mock_socket.recv.side_effect = recv_responses

    espota2.perform_ota(
        mock_socket,
        "testpass",
        mock_file,
        "partitions.bin",
        espota2.OTA_TYPE_UPDATE_PARTITION_TABLE,
    )

    # Verify magic bytes were sent
    assert mock_socket.sendall.call_args_list[0] == call(bytes(espota2.MAGIC_BYTES))

    # Verify features were sent (compression + SHA256 support + extended protocol)
    assert mock_socket.sendall.call_args_list[1] == call(
        bytes(
            [
                espota2.CLIENT_FEATURE_SUPPORTS_COMPRESSION
                | espota2.CLIENT_FEATURE_SUPPORTS_SHA256_AUTH
                | espota2.CLIENT_FEATURE_SUPPORTS_EXTENDED_PROTOCOL
                | espota2.CLIENT_FEATURE_SUPPORTS_DEFLATE
                | espota2.CLIENT_FEATURE_SUPPORTS_UDP
            ]
        )
    )

    # Verify ota type was sent
    assert mock_socket.sendall.call_args_list[2] == call(
        bytes([espota2.OTA_TYPE_UPDATE_PARTITION_TABLE])
    )


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_device_rejects_with_unsupported_ota_type(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """End-to-end: device returns 0x8E after the size byte; perform_ota must
    surface the human-readable 'unsupported OTA type' error from the lookup
    table in check_error()."""
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_FEATURE_FLAGS]),  # Extended protocol marker
        bytes(
            [
                espota2.SERVER_FEATURE_SUPPORTS_COMPRESSION
                | espota2.SERVER_FEATURE_SUPPORTS_PARTITION_ACCESS
            ]
        ),  # Feature flags
        bytes([espota2.RESPONSE_AUTH_OK]),  # No auth required
        bytes([espota2.RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE]),  # Reject at size step
    ]

    mock_socket.recv.side_effect = recv_responses

    with pytest.raises(
        espota2.OTAError,
        match="The requested OTA type is not supported by the device",
    ):
        espota2.perform_ota(
            mock_socket,
            "testpass",
            mock_file,
            "test.bin",
            espota2.OTA_TYPE_UPDATE_APP,
        )

    # Verify the client did send the OTA type byte before the size step
    assert mock_socket.sendall.call_args_list[2] == call(
        bytes([espota2.OTA_TYPE_UPDATE_APP])
    )


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_unsupported_type_rejected_early(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """ota_type values not in _SUPPORTED_OTA_TYPES are rejected before any I/O."""
    with pytest.raises(espota2.OTAError, match="Unsupported OTA type 0xFF"):
        espota2.perform_ota(
            mock_socket,
            "testpass",
            mock_file,
            "test.bin",
            0xFF,
        )
    # No bytes should have been transmitted to the device.
    mock_socket.sendall.assert_not_called()


@pytest.mark.parametrize("bad_type", [-1, 256, 0x10000, "app", None, 1.5])
def test_perform_ota_rejects_out_of_range_type(
    mock_socket: Mock, mock_file: io.BytesIO, bad_type: object
) -> None:
    """Out-of-range or non-int ota_type must raise OTAError, not ValueError."""
    with pytest.raises(espota2.OTAError, match="Invalid ota_type"):
        espota2.perform_ota(
            mock_socket,
            "testpass",
            mock_file,
            "test.bin",
            bad_type,  # type: ignore[arg-type]
        )
    mock_socket.sendall.assert_not_called()


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_non_app_type_requires_extended_protocol(
    mock_socket: Mock, mock_file: io.BytesIO, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Non-app OTA type must fail when device only supports the legacy protocol."""
    monkeypatch.setattr(
        espota2,
        "_SUPPORTED_OTA_TYPES",
        frozenset({espota2.OTA_TYPE_UPDATE_APP, 0xFF}),
    )
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_HEADER_OK]),  # Legacy single-byte feature ack
    ]

    mock_socket.recv.side_effect = recv_responses

    with pytest.raises(
        espota2.OTAError,
        match="Device does not support the extended OTA protocol",
    ):
        espota2.perform_ota(
            mock_socket,
            "testpass",
            mock_file,
            "test.bin",
            0xFF,
        )


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_non_app_type_requires_partition_access(
    mock_socket: Mock, mock_file: io.BytesIO, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Non-app OTA type must fail when device advertises extended protocol but
    not the partition-access feature."""
    monkeypatch.setattr(
        espota2,
        "_SUPPORTED_OTA_TYPES",
        frozenset({espota2.OTA_TYPE_UPDATE_APP, 0xFF}),
    )
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),  # First byte of version response
        bytes([espota2.OTA_VERSION_2_0]),  # Version number
        bytes([espota2.RESPONSE_FEATURE_FLAGS]),  # Extended protocol marker
        bytes(
            [espota2.SERVER_FEATURE_SUPPORTS_COMPRESSION]
        ),  # Compression only, no partition access
    ]

    mock_socket.recv.side_effect = recv_responses

    with pytest.raises(
        espota2.OTAError,
        match=(r"running firmware was built without 'allow_partition_access: true'"),
    ):
        espota2.perform_ota(
            mock_socket,
            "testpass",
            mock_file,
            "test.bin",
            0xFF,
        )


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_partition_access_error_names_bootloader_flag(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """Bootloader OTA against a stale device must point at the --bootloader flag."""
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),
        bytes([espota2.OTA_VERSION_2_0]),
        bytes([espota2.RESPONSE_FEATURE_FLAGS]),
        bytes([0]),  # No partition access
    ]

    mock_socket.recv.side_effect = recv_responses

    with pytest.raises(
        espota2.OTAError,
        match=r"--bootloader.*recompile and upload.*--bootloader.*retry --bootloader",
    ):
        espota2.perform_ota(
            mock_socket,
            "testpass",
            mock_file,
            "test.bin",
            espota2.OTA_TYPE_UPDATE_BOOTLOADER,
        )


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_partition_access_error_names_partition_table_flag(
    mock_socket: Mock, mock_file: io.BytesIO
) -> None:
    """Partition-table OTA against a stale device must point at the --partition-table flag."""
    recv_responses = [
        bytes([espota2.RESPONSE_OK]),
        bytes([espota2.OTA_VERSION_2_0]),
        bytes([espota2.RESPONSE_FEATURE_FLAGS]),
        bytes([0]),  # No partition access
    ]

    mock_socket.recv.side_effect = recv_responses

    with pytest.raises(
        espota2.OTAError,
        match=r"--partition-table.*retry --partition-table",
    ):
        espota2.perform_ota(
            mock_socket,
            "testpass",
            mock_file,
            "test.bin",
            espota2.OTA_TYPE_UPDATE_PARTITION_TABLE,
        )


def test_check_error_detects_errors_when_expect_is_none() -> None:
    """check_error must surface device error bytes even when expect is None.

    Regression test: previously, receive_exactly(..., expect=None) calls (used
    during feature negotiation and nonce reads) silently passed error bytes
    through, turning clean device errors into confusing later failures.
    """
    with pytest.raises(espota2.OTAError, match="Authentication invalid"):
        espota2.check_error([espota2.RESPONSE_ERROR_AUTH_INVALID], None)


def test_check_error_detects_empty_when_expect_is_none() -> None:
    """Empty data with expect=None must still raise (connection closed)."""
    with pytest.raises(
        espota2.OTAError, match="Device closed connection without responding"
    ):
        espota2.check_error([], None)


def test_check_error_passes_non_error_when_expect_is_none() -> None:
    """Non-error bytes with expect=None must pass through silently."""
    espota2.check_error([espota2.RESPONSE_OK], None)
    espota2.check_error([espota2.RESPONSE_HEADER_OK], None)
    espota2.check_error([espota2.RESPONSE_FEATURE_FLAGS], None)


# Device replies after the MD5 check for a one-chunk upload
_UPLOAD_TAIL = [
    bytes([espota2.RESPONSE_CHUNK_OK]),
    bytes([espota2.RESPONSE_RECEIVE_OK]),
    bytes([espota2.RESPONSE_UPDATE_END_OK]),
]


@pytest.mark.usefixtures("mock_time")
@pytest.mark.parametrize(
    "server_features",
    [
        espota2.SERVER_FEATURE_SUPPORTS_DEFLATE,
        # Binding offer: deflate wins over gzip
        espota2.SERVER_FEATURE_SUPPORTS_DEFLATE
        | espota2.SERVER_FEATURE_SUPPORTS_COMPRESSION,
    ],
)
def test_perform_ota_with_deflate(mock_socket: Mock, server_features: int) -> None:
    """The device gets a raw deflate stream, both sizes and the image MD5."""
    original_content = b"firmware" * 100
    mock_socket.recv.side_effect = (
        _no_auth_handshake(espota2.OTA_VERSION_2_0, server_features) + _UPLOAD_TAIL
    )

    espota2.perform_ota(mock_socket, None, io.BytesIO(original_content), "test.bin")

    sent = [c[0][0] for c in mock_socket.sendall.call_args_list]
    # magic, features, ota type, size, image size, md5, data, end ack
    sent_size = struct.unpack(">I", sent[3])[0]
    assert sent[4] == len(original_content).to_bytes(espota2.SIZE_FIELD_BYTES, "big")
    payload = sent[6]
    assert len(payload) == sent_size < len(original_content)
    assert zlib.decompress(payload, -espota2.DEFLATE_WINDOW_BITS) == original_content
    assert sent[5] == hashlib.md5(original_content).hexdigest().encode()


class _LossyUdpDevice:
    """The device side of the UDP channel (ota_esphome_udp.cpp) on a lossy link."""

    def __init__(self, token: bytes, loss: float, seed: int = 1) -> None:
        import random
        import threading

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.settimeout(0.05)
        self.token = token
        self.loss = loss
        self.random = random.Random(seed)
        self.next_seq = 0
        self.slots: dict[int, bytes] = {}  # early messages within the window
        self.received: list[bytes] = []
        self.out = bytearray()
        self.out_base = 0
        self.committed = False
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True)

    def __enter__(self) -> Self:
        self._thread.start()
        return self

    def __exit__(self, *exc: object) -> None:
        self._stop.set()
        self._thread.join()
        self.sock.close()

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                data, peer = self.sock.recvfrom(2048)
            except TimeoutError:
                continue
            if self.random.random() < self.loss:
                continue
            if data[1:5] != self.token:
                continue
            self.committed |= bool(data[0] & espota2.UDP_FLAG_COMMITTED)
            acked = (int.from_bytes(data[7:9], "big") - self.out_base) & 0xFFFF
            if acked <= len(self.out):
                del self.out[:acked]
                self.out_base += acked
            seq = int.from_bytes(data[5:7], "big")
            msg_type = data[0] & ~espota2.UDP_FLAG_COMMITTED
            if (
                msg_type == espota2.UDP_MSG_DATA
                and (seq - self.next_seq) & 0xFFFF < espota2.UDP_WINDOW
            ):
                self.slots.setdefault(seq, data[espota2.UDP_HEADER_SIZE :])
            # Consumed as soon as it is next, like a device that is never busy
            while self.next_seq in self.slots:
                self.received.append(self.slots.pop(self.next_seq))
                self.next_seq = (self.next_seq + 1) & 0xFFFF
            if self.random.random() < self.loss:
                continue
            window = sum(
                1 << k
                for k in range(1, espota2.UDP_WINDOW)
                if (self.next_seq + k) & 0xFFFF in self.slots
            )
            # Consumes at once, so every slot it does not hold is free
            window |= (espota2.UDP_WINDOW - len(self.slots)) << 4
            self.sock.sendto(
                bytes([espota2.UDP_MSG_ACK])
                + self.token
                + self.next_seq.to_bytes(2, "big")
                + seq.to_bytes(2, "big")
                + (self.out_base & 0xFFFF).to_bytes(2, "big")
                + bytes([window])
                + bytes(self.out),
                peer,
            )


def _udp_client(device: _LossyUdpDevice) -> espota2.UdpChannel:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.connect(device.sock.getsockname())
    return espota2.UdpChannel(sock, device.token)


@pytest.mark.parametrize("loss", [0.0, 0.5])
def test_udp_channel_delivers_in_order(loss: float) -> None:
    """The stream arrives intact and responses come back, despite loss."""
    data = bytes(range(256)) * 160
    with _LossyUdpDevice(b"\x01\x02\x03\x04", loss) as device:
        channel = _udp_client(device)
        assert channel.probe()
        channel.sendall(data)
        device.out += bytes([espota2.RESPONSE_RECEIVE_OK])
        assert channel.recv(1) == bytes([espota2.RESPONSE_RECEIVE_OK])
        channel.flush(30.0)
        channel.close()
    assert b"".join(device.received) == data
    assert all(len(m) <= espota2.UDP_MAX_PAYLOAD for m in device.received)
    assert device.committed


def test_start_udp_falls_back_when_blocked(mock_socket: Mock) -> None:
    """No answer to the probes sends the fallback over TCP and stays on TCP."""
    with _LossyUdpDevice(b"\x05\x06\x07\x08", 1.0) as device:
        mock_socket.family = socket.AF_INET
        mock_socket.getpeername.return_value = device.sock.getsockname()
        mock_socket.recv.side_effect = [b"\x05", b"\x06\x07\x08"]
        with patch.object(espota2, "UDP_PROBE_TIMEOUT", 0.3):
            assert espota2._start_udp(mock_socket, mock_socket, True) is None
    assert mock_socket.sendall.call_args_list == [
        call(espota2.UDP_REQUEST),
        call(espota2.UDP_FALLBACK),  # raw on the TCP socket
    ]


def test_start_udp_declined(mock_socket: Mock) -> None:
    """A zero token means the device could not open UDP; nothing is probed."""
    mock_socket.recv.side_effect = [b"\x00", b"\x00\x00\x00"]
    assert espota2._start_udp(mock_socket, mock_socket, True) is None
    assert mock_socket.sendall.call_args_list == [call(espota2.UDP_REQUEST)]


def test_start_udp_clean_link_stays_on_tcp(mock_socket: Mock) -> None:
    """A clean handshake asks to stay on TCP and opens no UDP socket."""
    mock_socket.recv.side_effect = [b"\x00", b"\x00\x00\x00"]
    with patch.object(espota2.socket, "socket") as udp_socket:
        assert espota2._start_udp(mock_socket, mock_socket, False) is None
    udp_socket.assert_not_called()
    assert mock_socket.sendall.call_args_list == [call(espota2.UDP_FALLBACK)]


@pytest.mark.parametrize(
    ("slowest", "prefer_udp", "retransmits", "lossy"),
    [
        (0.01, False, 0, False),
        (0.01, False, None, False),
        (espota2.UDP_SLOW_EXCHANGE + 0.1, False, 0, True),
        (0.01, True, 0, True),
        (0.01, False, 2, True),
    ],
)
def test_link_lossy(
    slowest: float, prefer_udp: bool, retransmits: int | None, lossy: bool
) -> None:
    """A slow exchange, a retransmit or an earlier failure means UDP."""
    with patch.object(espota2, "_tcp_retransmits", return_value=retransmits):
        assert espota2._link_lossy(Mock(), slowest, prefer_udp) is lossy


@pytest.mark.parametrize(
    ("platform", "info", "expected"),
    [
        ("linux", bytes(100) + (3).to_bytes(4, "little") + bytes(100), 3),
        ("darwin", bytes(72) + (1460).to_bytes(8, "little") + bytes(8), 1460),
        ("win32", b"", None),
        ("linux", bytes(10), None),
    ],
)
def test_tcp_retransmits(platform: str, info: bytes, expected: int | None) -> None:
    """The retransmit counter is read where the OS reports it."""
    sock = Mock()
    sock.getsockopt.return_value = info
    with patch("sys.platform", platform):
        assert espota2._tcp_retransmits(sock) == expected


def test_udp_channel_reports_socket_errors() -> None:
    """A closed device port fails fast instead of waiting out the timeout."""
    gone = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    gone.bind(("127.0.0.1", 0))
    addr = gone.getsockname()
    gone.close()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.connect(addr)
    channel = espota2.UdpChannel(sock, b"\x01\x01\x01\x01")
    channel.settimeout(10.0)
    with pytest.raises(espota2.OTANetworkError, match="closed its UDP port"):
        channel.sendall(bytes(espota2.UDP_MAX_PAYLOAD * 5))
    # A probe that is refused falls back instead
    assert not espota2.UdpChannel(sock, b"\x01\x01\x01\x01").probe()
    channel.close()


def test_udp_channel_no_progress_names_last_error() -> None:
    """Without refusals, the no-progress timeout names the last socket error."""
    sock = Mock()
    sock.send.side_effect = BlockingIOError(35, "full")
    channel = espota2.UdpChannel(sock, b"\x01\x01\x01\x01")
    channel.settimeout(0.2)
    channel.sendall(b"x")
    with (
        patch("select.select", return_value=([], [], [])),
        pytest.raises(espota2.OTANetworkError, match="no progress.*full"),
    ):
        while True:
            channel._pump()


def test_udp_channel_ack_resets_refusals() -> None:
    """Refusals only count in a row; an ACK in between resets them."""
    channel = espota2.UdpChannel(Mock(), b"\x01\x01\x01\x01")
    ack = bytes([espota2.UDP_MSG_ACK]) + b"\x01\x01\x01\x01" + bytes(6) + b"\x40"
    for _ in range(espota2.UDP_REFUSALS * 2):
        channel._socket_error(ConnectionRefusedError(61, "refused"))
        channel._handle_ack(ack)
    # Windows' form of the same refusal counts too
    with pytest.raises(espota2.OTANetworkError, match="closed its UDP port"):
        for _ in range(espota2.UDP_REFUSALS):
            channel._socket_error(ConnectionResetError(10054, "forcibly closed"))


def _udp_handshake() -> list[bytes]:
    """TCP recv responses up to auth, from a device that offers UDP."""
    return [
        bytes([espota2.RESPONSE_OK]),
        bytes([espota2.OTA_VERSION_2_0]),
        bytes([espota2.RESPONSE_FEATURE_FLAGS]),
        bytes([espota2.SERVER_FEATURE_SUPPORTS_UDP]),
        bytes([espota2.RESPONSE_AUTH_OK]),
    ]


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_over_udp(mock_socket: Mock) -> None:
    """The data phase moves to the channel, which acks chunks itself."""
    mock_socket.recv.side_effect = _udp_handshake()
    udp = Mock()
    udp.pending.return_value = False
    udp.recv.side_effect = [
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),
        bytes([espota2.RESPONSE_BIN_MD5_OK]),
        bytes([espota2.RESPONSE_RECEIVE_OK]),
        bytes([espota2.RESPONSE_UPDATE_END_OK]),
    ]
    with patch.object(espota2, "_start_udp", return_value=udp) as start:
        espota2.perform_ota(
            mock_socket, None, io.BytesIO(bytes(20000)), "test.bin", prefer_udp=True
        )
    assert start.call_args.args[2] is True  # an earlier failure means lossy
    assert udp.recv.call_count == 4  # no chunk acks
    udp.flush.assert_called_once()
    udp.close.assert_called_once()


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_over_udp_device_error(mock_socket: Mock) -> None:
    """A device error mid upload stops the upload and is not retried."""
    mock_socket.recv.side_effect = _udp_handshake()
    mock_socket.getsockopt.return_value = b""  # no retransmit counter
    udp = Mock()
    udp.pending.return_value = True
    udp.recv.side_effect = [
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),
        bytes([espota2.RESPONSE_BIN_MD5_OK]),
        bytes([espota2.RESPONSE_ERROR_WRITING_FLASH]),
    ]
    with (
        patch.object(espota2, "_start_udp", return_value=udp),
        pytest.raises(espota2.OTAError, match="Writing OTA data") as exc,
    ):
        espota2.perform_ota(mock_socket, None, io.BytesIO(bytes(20000)), "test.bin")
    assert not isinstance(exc.value, espota2.OTANetworkError)
    udp.close.assert_called()


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_udp_declined_stays_on_tcp(mock_socket: Mock) -> None:
    """Without a channel the upload continues on TCP with chunk acks."""
    mock_socket.recv.side_effect = [
        *_udp_handshake(),
        bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),
        bytes([espota2.RESPONSE_BIN_MD5_OK]),
        *_UPLOAD_TAIL,
    ]
    with patch.object(espota2, "_start_udp", return_value=None):
        espota2.perform_ota(
            mock_socket, None, io.BytesIO(b"x" * 100), "test.bin", prefer_udp=True
        )
    assert mock_socket.sendall.call_args_list[-1] == call(bytes([espota2.RESPONSE_OK]))


def test_udp_channel_records_socket_errors() -> None:
    """Send and receive errors are recorded like loss."""
    sock = Mock()
    sock.send.side_effect = BlockingIOError(35, "full")
    sock.recv.side_effect = ConnectionRefusedError(61, "refused")
    channel = espota2.UdpChannel(sock, b"\x01\x01\x01\x01")
    channel._send(espota2.UDP_MSG_PING, 0, b"")
    assert isinstance(channel._last_error, BlockingIOError)
    with patch("select.select", side_effect=[([sock], [], []), ([], [], [])]):
        channel._receive(0.0)
    assert isinstance(channel._last_error, ConnectionRefusedError)


def test_start_udp_probe_succeeds(mock_socket: Mock) -> None:
    """A device that answers the probe gets the data phase over UDP."""
    with _LossyUdpDevice(b"\x05\x06\x07\x09", 0.0) as device:
        mock_socket.family = socket.AF_INET
        mock_socket.getpeername.return_value = device.sock.getsockname()
        mock_socket.recv.side_effect = [b"\x05", b"\x06\x07\x09"]
        channel = espota2._start_udp(mock_socket, mock_socket, True)
        assert channel is not None
        channel.close()
    assert mock_socket.sendall.call_args_list == [call(espota2.UDP_REQUEST)]


def test_udp_channel_pings_when_all_held() -> None:
    """With every message in flight already held, it asks for responses instead,
    at most once per resend timeout."""
    device = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    device.bind(("127.0.0.1", 0))
    device.settimeout(1.0)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.connect(device.getsockname())
    channel = espota2.UdpChannel(sock, b"\x01\x01\x01\x01")
    channel.sendall(b"x")
    channel._queue[0].held = True
    with patch.object(channel, "_receive"):
        channel._pump()
        channel._pump()
    assert device.recv(2048)[0] == espota2.UDP_MSG_PING
    device.settimeout(0.1)
    with pytest.raises(TimeoutError):
        device.recv(2048)
    channel.close()
    device.close()


def test_udp_channel_ignores_foreign_datagrams() -> None:
    """Short, non-ACK or wrong-token datagrams change nothing."""
    channel = espota2.UdpChannel(Mock(), b"\x01\x01\x01\x01")
    for data in (b"\x40", bytes(espota2.UDP_ACK_HEADER_SIZE), b"\x40" + bytes(11)):
        channel._handle_ack(data)
    assert not channel.pending()
    # An ACK before the head was ever sent is no duplicate
    channel.sendall(b"x")
    ack = bytes([espota2.UDP_MSG_ACK]) + b"\x01\x01\x01\x01" + bytes(6) + b"\x40"
    channel._handle_ack(ack)
    channel._handle_ack(ack)
    assert not channel._queue[0].resend


def test_udp_channel_logs_each_socket_error_once(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A repeated error is recorded but logged only when it changes."""
    channel = espota2.UdpChannel(Mock(), b"\x01\x01\x01\x01")
    with caplog.at_level(logging.DEBUG, "esphome.espota2"):
        channel._socket_error(ConnectionRefusedError(61, "refused"))
        channel._socket_error(ConnectionRefusedError(61, "refused again"))
    assert caplog.text.count("UDP socket error") == 1
    assert "again" in str(channel._last_error)


def test_tcp_retransmits_unsupported_option() -> None:
    """An OS that rejects the option reports nothing."""
    sock = Mock()
    sock.getsockopt.side_effect = OSError("not supported")
    with patch("sys.platform", "linux"):
        assert espota2._tcp_retransmits(sock) is None


@pytest.mark.usefixtures("mock_socket_constructor", "mock_resolve_ip")
def test_run_ota_impl_handshake_failure_prefers_udp(
    firmware_file: Path, mock_perform_ota: Mock, mock_sleep: Mock
) -> None:
    """A handshake that dies after reaching the device moves the retry to UDP."""
    mock_perform_ota.side_effect = [
        espota2.OTAHandshakeNetworkError("noise handshake: timed out"),
        None,
    ]
    result_code, _ = espota2.run_ota_impl_("test.local", 3232, None, str(firmware_file))
    assert result_code == 0
    assert [c.kwargs["prefer_udp"] for c in mock_perform_ota.call_args_list] == [
        False,
        True,
    ]


def test_start_udp_socket_error_falls_back(mock_socket: Mock) -> None:
    """A UDP socket that cannot reach the peer stays on TCP."""
    mock_socket.family = socket.AF_INET
    mock_socket.getpeername.side_effect = OSError("not connected")
    mock_socket.recv.side_effect = [b"\x05", b"\x06\x07\x08"]
    assert espota2._start_udp(mock_socket, mock_socket, True) is None
    assert mock_socket.sendall.call_args_list == [
        call(espota2.UDP_REQUEST),
        call(espota2.UDP_FALLBACK),
    ]
