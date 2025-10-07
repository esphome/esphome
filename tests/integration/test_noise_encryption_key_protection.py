"""Integration test for noise encryption key protection from YAML."""

from __future__ import annotations

import base64

from aioesphomeapi import InvalidEncryptionKeyAPIError
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_noise_encryption_key_protection(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that noise encryption key set in YAML cannot be changed via API."""
    # The key that's set in the YAML fixture
    noise_psk = "zX9/JHxMKwpP0jUGsF0iESCm1wRvNgR6NkKVOhn7kSs="

    # Keep ESPHome process running throughout all tests
    async with run_compiled(yaml_config):
        # First connection - test key change attempt
        async with api_client_connected(noise_psk=noise_psk) as client:
            # Verify connection is established
            device_info = await client.device_info()
            assert device_info is not None

            # Try to set a new encryption key via API
            new_key = base64.b64encode(
                b"x" * 32
            )  # Valid 32-byte key in base64 as bytes

            # This should fail since key was set in YAML
            success = await client.noise_encryption_set_key(new_key)
            assert success is False

        # Reconnect with the original key to verify it still works
        async with api_client_connected(noise_psk=noise_psk) as client:
            # Verify connection is still successful with original key
            device_info = await client.device_info()
            assert device_info is not None
            assert device_info.name == "noise-key-test"

        # Verify that connecting with a wrong key fails
        wrong_key = base64.b64encode(b"y" * 32).decode()  # Different key
        with pytest.raises(InvalidEncryptionKeyAPIError):
            async with api_client_connected(noise_psk=wrong_key) as client:
                await client.device_info()
