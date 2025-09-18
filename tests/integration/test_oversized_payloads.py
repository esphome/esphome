"""Integration tests for oversized payloads and headers that should cause disconnection."""

from __future__ import annotations

import asyncio

import pytest

from .types import APIClientConnectedWithDisconnectFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_oversized_payload_plaintext(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected_with_disconnect: APIClientConnectedWithDisconnectFactory,
) -> None:
    """Test that oversized payloads (>100KiB) from client cause disconnection without crashing."""
    process_exited = False
    helper_log_found = False

    def check_logs(line: str) -> None:
        nonlocal process_exited, helper_log_found
        # Check for signs that the process exited/crashed
        if "Segmentation fault" in line or "core dumped" in line:
            process_exited = True
        # Check for HELPER_LOG message about message size exceeding maximum
        if (
            "[VV]" in line
            and "Bad packet: message size" in line
            and "exceeds maximum" in line
        ):
            helper_log_found = True

    async with run_compiled(yaml_config, line_callback=check_logs):
        async with api_client_connected_with_disconnect() as (client, disconnect_event):
            # Verify basic connection works first
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "oversized-plaintext"

            # Create an oversized payload (>100KiB)
            oversized_data = b"X" * (100 * 1024 + 1)  # 100KiB + 1 byte

            # Access the internal connection to send raw data
            frame_helper = client._connection._frame_helper
            # Create a message with oversized payload
            # Using message type 1 (DeviceInfoRequest) as an example
            message_type = 1
            frame_helper.write_packets([(message_type, oversized_data)], True)

            # Wait for the connection to be closed by ESPHome
            await asyncio.wait_for(disconnect_event.wait(), timeout=5.0)

        # After disconnection, verify process didn't crash
        assert not process_exited, "ESPHome process should not crash"
        # Verify we saw the expected HELPER_LOG message
        assert helper_log_found, (
            "Expected to see HELPER_LOG about message size exceeding maximum"
        )

        # Try to reconnect to verify the process is still running
        async with api_client_connected_with_disconnect() as (client2, _):
            device_info = await client2.device_info()
            assert device_info is not None
            assert device_info.name == "oversized-plaintext"


@pytest.mark.asyncio
async def test_oversized_protobuf_message_id_plaintext(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected_with_disconnect: APIClientConnectedWithDisconnectFactory,
) -> None:
    """Test that protobuf messages with ID > UINT16_MAX cause disconnection without crashing.

    This tests the message type limit - message IDs must fit in a uint16_t (0-65535).
    """
    process_exited = False
    helper_log_found = False

    def check_logs(line: str) -> None:
        nonlocal process_exited, helper_log_found
        # Check for signs that the process exited/crashed
        if "Segmentation fault" in line or "core dumped" in line:
            process_exited = True
        # Check for HELPER_LOG message about message type exceeding maximum
        if (
            "[VV]" in line
            and "Bad packet: message type" in line
            and "exceeds maximum" in line
        ):
            helper_log_found = True

    async with run_compiled(yaml_config, line_callback=check_logs):
        async with api_client_connected_with_disconnect() as (client, disconnect_event):
            # Verify basic connection works first
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "oversized-protobuf-plaintext"

            # Access the internal connection to send raw message with large ID
            frame_helper = client._connection._frame_helper
            # Message ID that exceeds uint16_t limit (> 65535)
            large_message_id = 65536  # 2^16, exceeds UINT16_MAX
            # Small payload for the test
            payload = b"test"

            # This should cause disconnection due to oversized varint
            frame_helper.write_packets([(large_message_id, payload)], True)

            # Wait for the connection to be closed by ESPHome
            await asyncio.wait_for(disconnect_event.wait(), timeout=5.0)

        # After disconnection, verify process didn't crash
        assert not process_exited, "ESPHome process should not crash"
        # Verify we saw the expected HELPER_LOG message
        assert helper_log_found, (
            "Expected to see HELPER_LOG about message type exceeding maximum"
        )

        # Try to reconnect to verify the process is still running
        async with api_client_connected_with_disconnect() as (client2, _):
            device_info = await client2.device_info()
            assert device_info is not None
            assert device_info.name == "oversized-protobuf-plaintext"


@pytest.mark.asyncio
async def test_oversized_payload_noise(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected_with_disconnect: APIClientConnectedWithDisconnectFactory,
) -> None:
    """Test that oversized payloads (>100KiB) from client cause disconnection without crashing with noise encryption."""
    noise_key = "N4Yle5YirwZhPiHHsdZLdOA73ndj/84veVaLhTvxCuU="
    process_exited = False
    cipherstate_failed = False

    def check_logs(line: str) -> None:
        nonlocal process_exited, cipherstate_failed
        # Check for signs that the process exited/crashed
        if "Segmentation fault" in line or "core dumped" in line:
            process_exited = True
        # Check for the expected warning about decryption failure
        if (
            "[W][api.connection" in line
            and "Reading failed CIPHERSTATE_DECRYPT_FAILED" in line
        ):
            cipherstate_failed = True

    async with run_compiled(yaml_config, line_callback=check_logs):
        async with api_client_connected_with_disconnect(noise_psk=noise_key) as (
            client,
            disconnect_event,
        ):
            # Verify basic connection works first
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "oversized-noise"

            # Create an oversized payload (>100KiB)
            oversized_data = b"Y" * (100 * 1024 + 1)  # 100KiB + 1 byte

            # Access the internal connection to send raw data
            frame_helper = client._connection._frame_helper
            # For noise connections, we still send through write_packets
            # but the frame helper will handle encryption
            # Using message type 1 (DeviceInfoRequest) as an example
            message_type = 1
            frame_helper.write_packets([(message_type, oversized_data)], True)

            # Wait for the connection to be closed by ESPHome
            await asyncio.wait_for(disconnect_event.wait(), timeout=5.0)

        # After disconnection, verify process didn't crash
        assert not process_exited, "ESPHome process should not crash"
        # Verify we saw the expected warning message
        assert cipherstate_failed, (
            "Expected to see warning about CIPHERSTATE_DECRYPT_FAILED"
        )

        # Try to reconnect to verify the process is still running
        async with api_client_connected_with_disconnect(noise_psk=noise_key) as (
            client2,
            _,
        ):
            device_info = await client2.device_info()
            assert device_info is not None
            assert device_info.name == "oversized-noise"


@pytest.mark.asyncio
async def test_oversized_protobuf_message_id_noise(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected_with_disconnect: APIClientConnectedWithDisconnectFactory,
) -> None:
    """Test that the noise protocol handles unknown message types correctly.

    With noise encryption, message types are stored as uint16_t (2 bytes) after decryption.
    Unknown message types should be ignored without disconnecting, as ESPHome needs to
    read the full message to maintain encryption stream continuity.
    """
    noise_key = "N4Yle5YirwZhPiHHsdZLdOA73ndj/84veVaLhTvxCuU="
    process_exited = False

    def check_logs(line: str) -> None:
        nonlocal process_exited
        # Check for signs that the process exited/crashed
        if "Segmentation fault" in line or "core dumped" in line:
            process_exited = True

    async with run_compiled(yaml_config, line_callback=check_logs):
        async with api_client_connected_with_disconnect(noise_psk=noise_key) as (
            client,
            disconnect_event,
        ):
            # Verify basic connection works first
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "oversized-noise"

            # With noise, message types are uint16_t, so we test with an unknown but valid value
            frame_helper = client._connection._frame_helper

            # Test with an unknown message type (65535 is not used by ESPHome)
            unknown_message_id = 65535  # Valid uint16_t but unknown to ESPHome
            payload = b"test"

            # Send the unknown message type - ESPHome should read and ignore it
            frame_helper.write_packets([(unknown_message_id, payload)], True)

            # Give ESPHome a moment to process (but expect no disconnection)
            # The connection should stay alive as ESPHome ignores unknown message types
            with pytest.raises(asyncio.TimeoutError):
                await asyncio.wait_for(disconnect_event.wait(), timeout=0.5)

            # Connection should still be alive - unknown types are ignored, not fatal
            assert client._connection.is_connected, (
                "Connection should remain open for unknown message types"
            )

            # Verify we can still communicate by sending a valid request
            device_info2 = await client.device_info()
            assert device_info2 is not None
            assert device_info2.name == "oversized-noise"

        # After test, verify process didn't crash
        assert not process_exited, "ESPHome process should not crash"

        # Verify we can still reconnect
        async with api_client_connected_with_disconnect(noise_psk=noise_key) as (
            client2,
            _,
        ):
            device_info = await client2.device_info()
            assert device_info is not None
            assert device_info.name == "oversized-noise"


@pytest.mark.asyncio
async def test_noise_corrupt_encrypted_frame(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected_with_disconnect: APIClientConnectedWithDisconnectFactory,
) -> None:
    """Test that noise protocol properly handles corrupt encrypted frames.

    Send a frame with valid size but corrupt encrypted content (garbage bytes).
    This should fail decryption and cause disconnection.
    """
    noise_key = "N4Yle5YirwZhPiHHsdZLdOA73ndj/84veVaLhTvxCuU="
    process_exited = False
    cipherstate_failed = False

    def check_logs(line: str) -> None:
        nonlocal process_exited, cipherstate_failed
        # Check for signs that the process exited/crashed
        if "Segmentation fault" in line or "core dumped" in line:
            process_exited = True
        # Check for the expected warning about decryption failure
        if (
            "[W][api.connection" in line
            and "Reading failed CIPHERSTATE_DECRYPT_FAILED" in line
        ):
            cipherstate_failed = True

    async with run_compiled(yaml_config, line_callback=check_logs):
        async with api_client_connected_with_disconnect(noise_psk=noise_key) as (
            client,
            disconnect_event,
        ):
            # Verify basic connection works first
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "oversized-noise"

            # Get the socket to send raw corrupt data
            socket = client._connection._socket

            # Send a corrupt noise frame directly to the socket
            # Format: [indicator=0x01][size_high][size_low][garbage_encrypted_data]
            # Size of 32 bytes (reasonable size for a noise frame with MAC)
            corrupt_frame = bytes(
                [
                    0x01,  # Noise indicator
                    0x00,  # Size high byte
                    0x20,  # Size low byte (32 bytes)
                ]
            ) + bytes(32)  # 32 bytes of zeros (invalid encrypted data)

            # Send the corrupt frame
            socket.sendall(corrupt_frame)

            # Wait for ESPHome to disconnect due to decryption failure
            await asyncio.wait_for(disconnect_event.wait(), timeout=5.0)

        # After disconnection, verify process didn't crash
        assert not process_exited, (
            "ESPHome process should not crash on corrupt encrypted frames"
        )
        # Verify we saw the expected warning message
        assert cipherstate_failed, (
            "Expected to see warning about CIPHERSTATE_DECRYPT_FAILED"
        )

        # Verify we can still reconnect after handling the corrupt frame
        async with api_client_connected_with_disconnect(noise_psk=noise_key) as (
            client2,
            _,
        ):
            device_info = await client2.device_info()
            assert device_info is not None
            assert device_info.name == "oversized-noise"
