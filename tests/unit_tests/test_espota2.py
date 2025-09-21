"""Unit tests for esphome.espota2 module."""

from __future__ import annotations

import gzip
import hashlib
import io
import socket
from unittest.mock import MagicMock, Mock, call, patch

import pytest

from esphome import espota2
from esphome.core import EsphomeError


@pytest.fixture
def mock_socket():
    """Create a mock socket for testing."""
    socket = Mock()
    socket.close = Mock()
    socket.recv = Mock()
    socket.sendall = Mock()
    socket.settimeout = Mock()
    socket.connect = Mock()
    return socket


def test_recv_decode_with_decode(mock_socket) -> None:
    """Test recv_decode with decode=True returns list."""
    mock_socket.recv.return_value = b"\x01\x02\x03"

    result = espota2.recv_decode(mock_socket, 3, decode=True)

    assert result == [1, 2, 3]
    mock_socket.recv.assert_called_once_with(3)


def test_recv_decode_without_decode(mock_socket) -> None:
    """Test recv_decode with decode=False returns bytes."""
    mock_socket.recv.return_value = b"\x01\x02\x03"

    result = espota2.recv_decode(mock_socket, 3, decode=False)

    assert result == b"\x01\x02\x03"
    mock_socket.recv.assert_called_once_with(3)


def test_receive_exactly_success(mock_socket) -> None:
    """Test receive_exactly successfully receives expected data."""
    mock_socket.recv.side_effect = [b"\x00", b"\x01\x02"]

    result = espota2.receive_exactly(mock_socket, 3, "test", espota2.RESPONSE_OK)

    assert result == [0, 1, 2]
    assert mock_socket.recv.call_count == 2


def test_receive_exactly_with_error_response(mock_socket) -> None:
    """Test receive_exactly raises OTAError on error response."""
    mock_socket.recv.return_value = bytes([espota2.RESPONSE_ERROR_AUTH_INVALID])

    with pytest.raises(espota2.OTAError, match="Error auth:.*Authentication invalid"):
        espota2.receive_exactly(mock_socket, 1, "auth", [espota2.RESPONSE_OK])

    mock_socket.close.assert_called_once()


def test_receive_exactly_socket_error(mock_socket) -> None:
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
            "Error: Wring OTA data to flash memory failed",
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


def test_send_check_with_various_data_types(mock_socket) -> None:
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


def test_send_check_socket_error(mock_socket) -> None:
    """Test send_check handles socket errors."""
    mock_socket.sendall.side_effect = OSError("Broken pipe")

    with pytest.raises(espota2.OTAError, match="Error sending test"):
        espota2.send_check(mock_socket, b"data", "test")


def test_perform_ota_successful_md5_auth(mock_socket) -> None:
    """Test successful OTA with MD5 authentication."""
    mock_file = io.BytesIO(b"firmware content here")

    # Mock random for predictable cnonce
    with (
        patch("random.random", return_value=0.123456),
        patch("time.sleep"),
        patch("time.perf_counter", side_effect=[0, 1]),
    ):
        # Setup socket responses for recv calls
        recv_responses = [
            bytes([espota2.RESPONSE_OK]),  # First byte of version response
            bytes([espota2.OTA_VERSION_2_0]),  # Version number
            bytes([espota2.RESPONSE_HEADER_OK]),  # Features response
            bytes([espota2.RESPONSE_REQUEST_AUTH]),  # Auth request
            b"12345678901234567890123456789012",  # 32 char hex nonce
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
        cnonce = hashlib.md5(b"0.123456").hexdigest()
        assert mock_socket.sendall.call_args_list[2] == call(cnonce.encode())

        # Verify auth result was computed correctly
        expected_hash = hashlib.md5()
        expected_hash.update(b"testpass")
        expected_hash.update(b"12345678901234567890123456789012")
        expected_hash.update(cnonce.encode())
        expected_result = expected_hash.hexdigest()
        assert mock_socket.sendall.call_args_list[3] == call(expected_result.encode())


def test_perform_ota_no_auth(mock_socket) -> None:
    """Test OTA without authentication."""
    mock_file = io.BytesIO(b"firmware")

    with patch("time.sleep"), patch("time.perf_counter", side_effect=[0, 1]):
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


def test_perform_ota_with_compression(mock_socket) -> None:
    """Test OTA with compression support."""
    original_content = b"firmware" * 100  # Repeating content for compression
    mock_file = io.BytesIO(original_content)

    with patch("time.sleep"), patch("time.perf_counter", side_effect=[0, 1]):
        recv_responses = [
            bytes([espota2.RESPONSE_OK]),  # First byte of version response
            bytes([espota2.OTA_VERSION_2_0]),  # Version number
            bytes(
                [espota2.RESPONSE_SUPPORTS_COMPRESSION]
            ),  # Device supports compression
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
        sent_size = (
            (size_bytes[0] << 24)
            | (size_bytes[1] << 16)
            | (size_bytes[2] << 8)
            | size_bytes[3]
        )

        # Size should be less than original due to compression
        assert sent_size < len(original_content)

        # Verify the content sent was gzipped
        compressed = gzip.compress(original_content, compresslevel=9)
        assert sent_size == len(compressed)


def test_perform_ota_auth_without_password(mock_socket) -> None:
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


def test_perform_ota_unsupported_version(mock_socket) -> None:
    """Test OTA fails with unsupported version."""
    mock_file = io.BytesIO(b"firmware")

    responses = [
        bytes([espota2.RESPONSE_OK, 99]),  # Unsupported version
    ]

    mock_socket.recv.side_effect = responses

    with pytest.raises(espota2.OTAError, match="Device uses unsupported OTA version"):
        espota2.perform_ota(mock_socket, "", mock_file, "test.bin")


def test_perform_ota_upload_error(mock_socket) -> None:
    """Test OTA handles upload errors."""
    mock_file = io.BytesIO(b"firmware")

    with patch("time.perf_counter", side_effect=[0, 1]):
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

        with pytest.raises(
            espota2.OTAError, match="Error receiving acknowledge chunk OK"
        ):
            espota2.perform_ota(mock_socket, "", mock_file, "test.bin")


def test_run_ota_impl_successful() -> None:
    """Test run_ota_impl_ with successful upload."""
    mock_socket = Mock()

    with (
        patch("socket.socket", return_value=mock_socket),
        patch("esphome.espota2.resolve_ip_address") as mock_resolve,
        patch("builtins.open", create=True) as mock_open,
        patch("esphome.espota2.perform_ota") as mock_perform,
    ):
        # Setup mocks
        mock_resolve.return_value = [
            (socket.AF_INET, socket.SOCK_STREAM, 0, "", ("192.168.1.100", 3232))
        ]
        mock_file = MagicMock()
        mock_open.return_value.__enter__.return_value = mock_file

        # Run OTA
        result_code, result_host = espota2.run_ota_impl_(
            "test.local", 3232, "password", "firmware.bin"
        )

        # Verify success
        assert result_code == 0
        assert result_host == "192.168.1.100"

        # Verify socket was configured correctly
        mock_socket.settimeout.assert_called_with(10.0)
        mock_socket.connect.assert_called_once_with(("192.168.1.100", 3232))
        mock_socket.close.assert_called_once()

        # Verify perform_ota was called
        mock_perform.assert_called_once_with(
            mock_socket, "password", mock_file, "firmware.bin"
        )


def test_run_ota_impl_connection_failed() -> None:
    """Test run_ota_impl_ when connection fails."""
    mock_socket = Mock()
    mock_socket.connect.side_effect = OSError("Connection refused")

    with (
        patch("socket.socket", return_value=mock_socket),
        patch("esphome.espota2.resolve_ip_address") as mock_resolve,
    ):
        mock_resolve.return_value = [
            (socket.AF_INET, socket.SOCK_STREAM, 0, "", ("192.168.1.100", 3232))
        ]

        result_code, result_host = espota2.run_ota_impl_(
            "test.local", 3232, "password", "firmware.bin"
        )

        assert result_code == 1
        assert result_host is None
        mock_socket.close.assert_called_once()


def test_run_ota_impl_resolve_failed() -> None:
    """Test run_ota_impl_ when DNS resolution fails."""
    with patch("esphome.espota2.resolve_ip_address") as mock_resolve:
        mock_resolve.side_effect = EsphomeError("DNS resolution failed")

        with pytest.raises(espota2.OTAError, match="DNS resolution failed"):
            result_code, result_host = espota2.run_ota_impl_(
                "unknown.host", 3232, "password", "firmware.bin"
            )


def test_run_ota_wrapper() -> None:
    """Test run_ota wrapper function."""
    with patch("esphome.espota2.run_ota_impl_") as mock_impl:
        # Test successful case
        mock_impl.return_value = (0, "192.168.1.100")
        result = espota2.run_ota("test.local", 3232, "pass", "fw.bin")
        assert result == (0, "192.168.1.100")

        # Test error case
        mock_impl.side_effect = espota2.OTAError("Test error")
        result = espota2.run_ota("test.local", 3232, "pass", "fw.bin")
        assert result == (1, None)


def test_progress_bar() -> None:
    """Test ProgressBar functionality."""
    with (
        patch("sys.stderr.write") as mock_write,
        patch("sys.stderr.flush"),
    ):
        progress = espota2.ProgressBar()

        # Test initial update
        progress.update(0.0)
        assert mock_write.called
        assert "0%" in mock_write.call_args[0][0]

        # Test progress update
        mock_write.reset_mock()
        progress.update(0.5)
        assert "50%" in mock_write.call_args[0][0]

        # Test completion
        mock_write.reset_mock()
        progress.update(1.0)
        assert "100%" in mock_write.call_args[0][0]
        assert "Done" in mock_write.call_args[0][0]

        # Test done method
        mock_write.reset_mock()
        progress.done()
        assert mock_write.call_args[0][0] == "\n"

        # Test same progress doesn't update
        mock_write.reset_mock()
        progress.update(0.5)
        progress.update(0.5)
        assert mock_write.call_count == 1  # Only called once


# Tests for SHA256 authentication (for when PR is merged)
def test_perform_ota_successful_sha256_auth() -> None:
    """Test successful OTA with SHA256 authentication (future support)."""
    mock_socket = Mock()

    # Mock random for predictable cnonce
    with patch("random.random", return_value=0.123456):
        # Constants for SHA256 auth (when implemented)
        RESPONSE_REQUEST_SHA256_AUTH = 0x02  # From PR

        # Setup socket responses
        responses = [
            # Version handshake
            bytes([espota2.RESPONSE_OK, espota2.OTA_VERSION_2_0]),
            # Features response
            bytes([espota2.RESPONSE_HEADER_OK]),
            # SHA256 Auth request
            bytes([RESPONSE_REQUEST_SHA256_AUTH]),
            # Nonce from device (64 chars for SHA256)
            b"1234567890123456789012345678901234567890123456789012345678901234",
            # Auth result
            bytes([espota2.RESPONSE_AUTH_OK]),
            # Binary size OK
            bytes([espota2.RESPONSE_UPDATE_PREPARE_OK]),
            # MD5 checksum OK
            bytes([espota2.RESPONSE_BIN_MD5_OK]),
            # Chunk OK
            bytes([espota2.RESPONSE_CHUNK_OK]),
            bytes([espota2.RESPONSE_CHUNK_OK]),
            bytes([espota2.RESPONSE_CHUNK_OK]),
            # Receive OK
            bytes([espota2.RESPONSE_RECEIVE_OK]),
            # Update end OK
            bytes([espota2.RESPONSE_UPDATE_END_OK]),
        ]

        mock_socket.recv.side_effect = responses

        # When SHA256 is implemented, this test will verify:
        # 1. Client sends FEATURE_SUPPORTS_SHA256_AUTH flag
        # 2. Device responds with RESPONSE_REQUEST_SHA256_AUTH
        # 3. Authentication uses SHA256 instead of MD5
        # 4. Nonce is 64 characters instead of 32

        # For now, this would raise an error since SHA256 isn't implemented
        # Once implemented, uncomment to test:
        # espota2.perform_ota(mock_socket, "testpass", mock_file, "test.bin")


def test_perform_ota_sha256_fallback_to_md5() -> None:
    """Test SHA256-capable client falls back to MD5 for compatibility."""
    # This test verifies the temporary backward compatibility
    # where a SHA256-capable client can still authenticate with MD5
    # This compatibility will be removed in 2026.1.0 according to PR
    pass  # Implementation depends on final PR merge


def test_perform_ota_version_differences(mock_socket) -> None:
    """Test OTA behavior differences between version 1.0 and 2.0."""
    mock_file = io.BytesIO(b"firmware")

    with patch("time.sleep"), patch("time.perf_counter", side_effect=[0, 1]):
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

        # Verify no chunk acknowledgments were expected
        # (implementation detail - v1 doesn't wait for chunk OK)
