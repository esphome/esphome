"""Unit tests for esphome.espota2 module."""

from __future__ import annotations

from collections.abc import Generator
import gzip
import hashlib
import io
from pathlib import Path
import socket
import struct
from unittest.mock import Mock, call, patch

import pytest
from pytest import CaptureFixture

from esphome import espota2
from esphome.core import EsphomeError

# Test constants
MOCK_RANDOM_VALUE = 0.123456
MOCK_RANDOM_BYTES = b"0.123456"
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
def mock_time() -> Generator[None]:
    """Mock time-related functions for consistent testing."""
    # Provide enough values for multiple calls (tests may call perform_ota multiple times)
    with (
        patch("time.sleep"),
        patch("time.perf_counter", side_effect=[0, 1, 0, 1, 0, 1]),
    ):
        yield


@pytest.fixture
def mock_random() -> Generator[Mock]:
    """Mock random for predictable test values."""
    with patch("random.random", return_value=MOCK_RANDOM_VALUE) as mock_rand:
        yield mock_rand


@pytest.fixture
def mock_resolve_ip() -> Generator[Mock]:
    """Mock resolve_ip_address for testing."""
    with patch("esphome.espota2.resolve_ip_address") as mock:
        mock.return_value = [
            (socket.AF_INET, socket.SOCK_STREAM, 0, "", ("192.168.1.100", 3232))
        ]
        yield mock


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

    with pytest.raises(espota2.OTAError, match="Error auth:.*Authentication invalid"):
        espota2.receive_exactly(mock_socket, 1, "auth", [espota2.RESPONSE_OK])

    mock_socket.close.assert_called_once()


def test_receive_exactly_socket_error(mock_socket: Mock) -> None:
    """Test receive_exactly handles socket errors."""
    mock_socket.recv.side_effect = OSError("Connection reset")

    with pytest.raises(espota2.OTAError, match="Error receiving acknowledge test"):
        espota2.receive_exactly(mock_socket, 1, "test", espota2.RESPONSE_OK)


@pytest.mark.parametrize(
    ("error_code", "expected_msg"),
    [
        (espota2.RESPONSE_ERROR_MAGIC, "Error: Invalid magic byte"),
        (espota2.RESPONSE_ERROR_UPDATE_PREPARE, "Error: Couldn't prepare flash memory"),
        (espota2.RESPONSE_ERROR_AUTH_INVALID, "Error: Authentication invalid"),
        (
            espota2.RESPONSE_ERROR_WRITING_FLASH,
            "Error: Writing OTA data to flash memory failed",
        ),
        (espota2.RESPONSE_ERROR_UPDATE_END, "Error: Finishing update failed"),
        (
            espota2.RESPONSE_ERROR_INVALID_BOOTSTRAPPING,
            "Error: Please press the reset button",
        ),
        (
            espota2.RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG,
            "Error: ESP has been flashed with wrong flash size",
        ),
        (
            espota2.RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG,
            "Error: ESP does not have the requested flash size",
        ),
        (
            espota2.RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE,
            "Error: ESP does not have enough space",
        ),
        (
            espota2.RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE,
            "Error: The OTA partition on the ESP is too small",
        ),
        (
            espota2.RESPONSE_ERROR_NO_UPDATE_PARTITION,
            "Error: The OTA partition on the ESP couldn't be found",
        ),
        (espota2.RESPONSE_ERROR_MD5_MISMATCH, "Error: Application MD5 code mismatch"),
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

    with pytest.raises(espota2.OTAError, match="Error sending test"):
        espota2.send_check(mock_socket, b"data", "test")


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_successful_md5_auth(
    mock_socket: Mock, mock_file: io.BytesIO, mock_random: Mock
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

    # Verify features were sent (compression + SHA256 support)
    assert mock_socket.sendall.call_args_list[1] == call(
        bytes(
            [
                espota2.FEATURE_SUPPORTS_COMPRESSION
                | espota2.FEATURE_SUPPORTS_SHA256_AUTH
            ]
        )
    )

    # Verify cnonce was sent (MD5 of random.random())
    cnonce = hashlib.md5(MOCK_RANDOM_BYTES).hexdigest()
    assert mock_socket.sendall.call_args_list[2] == call(cnonce.encode())

    # Verify auth result was computed correctly
    expected_hash = hashlib.md5()
    expected_hash.update(b"testpass")
    expected_hash.update(MOCK_MD5_NONCE)
    expected_hash.update(cnonce.encode())
    expected_result = expected_hash.hexdigest()
    assert mock_socket.sendall.call_args_list[3] == call(expected_result.encode())


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_no_auth(mock_socket: Mock, mock_file: io.BytesIO) -> None:
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

    espota2.perform_ota(mock_socket, "", mock_file, "test.bin")

    # Should not send any auth-related data
    auth_calls = [
        call
        for call in mock_socket.sendall.call_args_list
        if "cnonce" in str(call) or "result" in str(call)
    ]
    assert len(auth_calls) == 0


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

    espota2.perform_ota(mock_socket, "", mock_file, "test.bin")

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
        espota2.perform_ota(mock_socket, "", mock_file, "test.bin")


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_md5_auth_wrong_password(
    mock_socket: Mock, mock_file: io.BytesIO, mock_random: Mock
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

    with pytest.raises(espota2.OTAError, match="Error auth.*Authentication invalid"):
        espota2.perform_ota(mock_socket, "wrongpassword", mock_file, "test.bin")

    # Verify the socket was closed after auth failure
    mock_socket.close.assert_called()


@pytest.mark.usefixtures("mock_time")
def test_perform_ota_sha256_auth_wrong_password(
    mock_socket: Mock, mock_file: io.BytesIO, mock_random: Mock
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

    with pytest.raises(espota2.OTAError, match="Error auth.*Authentication invalid"):
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
        espota2.perform_ota(mock_socket, "", mock_file, "test.bin")


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
        espota2.OTAError, match=r"Error auth: Unexpected response from ESP: 0x03"
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
        espota2.perform_ota(mock_socket, "", mock_file, "test.bin")


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

    with pytest.raises(espota2.OTAError, match="Error receiving acknowledge chunk OK"):
        espota2.perform_ota(mock_socket, "", mock_file, "test.bin")


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
    mock_socket.settimeout.assert_called_with(20.0)
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
def test_run_ota_impl_connection_failed(mock_socket: Mock, tmp_path: Path) -> None:
    """Test run_ota_impl_ when connection fails."""
    mock_socket.connect.side_effect = OSError("Connection refused")

    # Create a real firmware file
    firmware_file = tmp_path / "firmware.bin"
    firmware_file.write_bytes(b"firmware content")

    result_code, result_host = espota2.run_ota_impl_(
        "test.local", 3232, "password", str(firmware_file)
    )

    assert result_code == 1
    assert result_host is None
    mock_socket.close.assert_called_once()


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
    progress = espota2.ProgressBar()

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

    # Test done method
    progress.done()
    captured = capsys.readouterr()
    assert captured.err == "\n"

    # Test same progress doesn't update
    progress.update(0.5)
    progress.update(0.5)
    captured = capsys.readouterr()
    # Should only see one update (second call shouldn't write)
    assert captured.err.count("50%") == 1


# Tests for SHA256 authentication
@pytest.mark.usefixtures("mock_time")
def test_perform_ota_successful_sha256_auth(
    mock_socket: Mock, mock_file: io.BytesIO, mock_random: Mock
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

    # Verify features were sent (compression + SHA256 support)
    assert mock_socket.sendall.call_args_list[1] == call(
        bytes(
            [
                espota2.FEATURE_SUPPORTS_COMPRESSION
                | espota2.FEATURE_SUPPORTS_SHA256_AUTH
            ]
        )
    )

    # Verify cnonce was sent (SHA256 of random.random())
    cnonce = hashlib.sha256(MOCK_RANDOM_BYTES).hexdigest()
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
    mock_socket: Mock, mock_file: io.BytesIO, mock_random: Mock
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
                espota2.FEATURE_SUPPORTS_COMPRESSION
                | espota2.FEATURE_SUPPORTS_SHA256_AUTH
            ]
        )
    )

    # But authentication was done with MD5
    cnonce = hashlib.md5(MOCK_RANDOM_BYTES).hexdigest()
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
    espota2.perform_ota(mock_socket, "", mock_file, "test.bin")

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
    espota2.perform_ota(mock_socket, "", mock_file, "test.bin")

    # For v2.0, verify more recv calls due to chunk acknowledgments
    assert mock_socket.recv.call_count == 9  # v2.0 has 9 recv calls (includes chunk OK)
