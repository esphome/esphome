"""Integration tests for protobuf bounds check integer overflow fix (GHSA-4h3h-63v6-88qx).

This tests the fix for CVE where an integer overflow in the comparison
`ptr + field_length > end` could be bypassed by sending a large field_length value,
causing the device to crash by reading out-of-bounds memory.

The fix changes the comparison to `field_length > static_cast<size_t>(end - ptr)`
which avoids the overflow by comparing against the remaining buffer size directly.
"""

from __future__ import annotations

import asyncio
import contextlib
import socket

import pytest

from .const import LOCALHOST
from .types import APIClientConnectedWithDisconnectFactory, RunCompiledFunction


def _encode_varint(value: int) -> bytes:
    """Encode an integer as a protobuf varint."""
    result = []
    while value > 127:
        result.append((value & 0x7F) | 0x80)
        value >>= 7
    result.append(value & 0x7F)
    return bytes(result)


def _create_malicious_hello_request(field_length: int) -> bytes:
    """Create a malicious HelloRequest packet with overflow-inducing field_length.

    The packet structure is:
    - 0x00: Plaintext protocol indicator
    - VarInt: Total message size
    - 0x01: Message type (HelloRequest)
    - 0x02: Field tag (field_id=0, wire_type=2 LENGTH_DELIMITED)
    - VarInt: field_length (the malicious value)

    When field_length is large (e.g., 0xe0000000), on 32-bit systems the comparison
    `ptr + field_length > end` would overflow, bypassing the bounds check.
    """
    field_length_varint = _encode_varint(field_length)
    # Message content: field tag (0x02) + field_length varint
    message_content = bytes([0x02]) + field_length_varint
    # Full message: message type (0x01) + content
    full_message = bytes([0x01]) + message_content
    # Size varint
    size_varint = _encode_varint(len(full_message))
    # Complete packet: indicator (0x00) + size + message
    return bytes([0x00]) + size_varint + full_message


def _send_malicious_packets_raw(host: str, port: int, packets: list[bytes]) -> None:
    """Send malicious packets using a raw socket connection.

    This bypasses the aioesphomeapi client to send raw malformed data directly
    to the ESPHome API server.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    try:
        sock.connect((host, port))
        for packet in packets:
            sock.sendall(packet)
    except (TimeoutError, ConnectionResetError, BrokenPipeError):
        # Expected - server may close connection after malformed packet
        pass
    finally:
        sock.close()


@pytest.mark.asyncio
async def test_proto_bounds_check_overflow_plaintext(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected_with_disconnect: APIClientConnectedWithDisconnectFactory,
    unused_tcp_port: int,
) -> None:
    """Test that protobuf bounds check overflow doesn't crash the device (plaintext).

    This tests the fix for GHSA-4h3h-63v6-88qx where sending a HelloRequest
    with a large field_length could cause an integer overflow in the bounds check,
    leading to out-of-bounds memory access and device crash.

    The attack works by sending a packet where field_length is large enough that
    `ptr + field_length` wraps around to a smaller value, bypassing the > end check.
    """
    process_crashed = False
    invalid_length_logged = False

    def check_logs(line: str) -> None:
        nonlocal process_crashed, invalid_length_logged
        # Check for signs that the process crashed
        if "Segmentation fault" in line or "core dumped" in line:
            process_crashed = True
        # Check if the bounds check caught the malicious packet
        if "Out-of-bounds Length Delimited" in line:
            invalid_length_logged = True

    async with run_compiled(yaml_config, line_callback=check_logs):
        # First verify the API is working normally
        async with api_client_connected_with_disconnect() as (client, _):
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "proto-overflow-plaintext"

        # Now send malicious packets using raw socket
        # Test with multiple field_length values that would cause overflow on 32-bit
        # These values are chosen to cause ptr + field_length to wrap around
        overflow_values = [
            0xE0000000,  # Causes crash on ESP32 and RPi Pico W
            0xD0000000,  # Crashes ESP32
            0xF0000000,  # May not crash but reads unrelated memory
            0xFFFFFFFF,  # Maximum uint32 value
        ]

        malicious_packets = [
            _create_malicious_hello_request(val) for val in overflow_values
        ]

        # Send malicious packets in executor to not block event loop
        loop = asyncio.get_running_loop()
        await loop.run_in_executor(
            None,
            _send_malicious_packets_raw,
            LOCALHOST,
            unused_tcp_port,
            malicious_packets,
        )

        # Small delay to let ESPHome process the packets
        await asyncio.sleep(0.5)

        # After the malicious packets, verify the process didn't crash
        assert not process_crashed, (
            "ESPHome process crashed! The bounds check overflow fix is not working."
        )

        # Most importantly: verify we can reconnect, proving the process is still running
        async with api_client_connected_with_disconnect() as (client2, _):
            device_info = await client2.device_info()
            assert device_info is not None
            assert device_info.name == "proto-overflow-plaintext"


@pytest.mark.asyncio
async def test_proto_bounds_check_overflow_noise(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected_with_disconnect: APIClientConnectedWithDisconnectFactory,
) -> None:
    """Test that protobuf bounds check overflow doesn't crash the device (noise encryption).

    With noise encryption, the attack requires knowledge of the encryption key.
    This test verifies that even with a valid encryption session, malicious
    protobuf content doesn't crash the device.
    """
    noise_key = "N4Yle5YirwZhPiHHsdZLdOA73ndj/84veVaLhTvxCuU="
    process_crashed = False

    def check_logs(line: str) -> None:
        nonlocal process_crashed
        if "Segmentation fault" in line or "core dumped" in line:
            process_crashed = True

    async with run_compiled(yaml_config, line_callback=check_logs):
        async with api_client_connected_with_disconnect(noise_psk=noise_key) as (
            client,
            disconnect_event,
        ):
            # Verify basic connection works first
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "proto-overflow-noise"

            # With noise encryption, we need to send through the frame helper
            # which will encrypt the data. We'll send a message with a malformed
            # protobuf body that has a large length-delimited field.
            frame_helper = client._connection._frame_helper

            # Create a malformed protobuf body with overflow-inducing field length
            # This is the content after encryption/decryption
            # Tag 0x02 (field_id=0, wire_type=2) followed by large length
            malformed_bodies = [
                bytes([0x02]) + _encode_varint(0xE0000000),  # Overflow value
                bytes([0x02]) + _encode_varint(0xFFFFFFFF),  # Max uint32
            ]

            for body in malformed_bodies:
                # Send as HelloRequest (type 1)
                try:
                    frame_helper.write_packets([(1, body)], True)
                except (ConnectionResetError, BrokenPipeError, OSError):
                    # Connection may be closed after malformed packet
                    break
                await asyncio.sleep(0.1)

            # Wait briefly for any disconnect
            with contextlib.suppress(TimeoutError):
                await asyncio.wait_for(disconnect_event.wait(), timeout=1.0)

        # Verify process didn't crash
        assert not process_crashed, (
            "ESPHome process crashed! The bounds check overflow fix is not working."
        )

        # Verify we can reconnect
        async with api_client_connected_with_disconnect(noise_psk=noise_key) as (
            client2,
            _,
        ):
            device_info = await client2.device_info()
            assert device_info is not None
            assert device_info.name == "proto-overflow-noise"


@pytest.mark.asyncio
async def test_proto_fixed32_bounds_check_plaintext(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected_with_disconnect: APIClientConnectedWithDisconnectFactory,
    unused_tcp_port: int,
) -> None:
    """Test that fixed32 bounds check works correctly.

    This tests the simpler case where we check if there are 4 bytes remaining.
    While less likely to overflow, the fix ensures consistent bounds checking.
    """
    process_crashed = False

    def check_logs(line: str) -> None:
        nonlocal process_crashed
        if "Segmentation fault" in line or "core dumped" in line:
            process_crashed = True

    async with run_compiled(yaml_config, line_callback=check_logs):
        # First verify the API is working normally
        async with api_client_connected_with_disconnect() as (client, _):
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "proto-overflow-plaintext"

        # Create a packet with a fixed32 field (wire type 5) but truncated data
        # Tag: field_id=1, wire_type=5 (fixed32) = (1 << 3) | 5 = 0x0D
        # This should be caught by the bounds check
        truncated_fixed32 = bytes(
            [
                0x00,  # Plaintext indicator
                0x03,  # Size (3 bytes of message)
                0x01,  # Message type (HelloRequest)
                0x0D,  # Field tag (field_id=1, wire_type=5 fixed32)
                0x42,  # Only 1 byte of data instead of 4
            ]
        )

        # Send using raw socket
        loop = asyncio.get_running_loop()
        await loop.run_in_executor(
            None,
            _send_malicious_packets_raw,
            LOCALHOST,
            unused_tcp_port,
            [truncated_fixed32],
        )

        await asyncio.sleep(0.5)

        assert not process_crashed, "ESPHome process crashed on truncated fixed32!"

        # Verify we can still reconnect
        async with api_client_connected_with_disconnect() as (client2, _):
            device_info = await client2.device_info()
            assert device_info is not None
            assert device_info.name == "proto-overflow-plaintext"
