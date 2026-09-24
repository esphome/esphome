"""Integration test for Host mode with noise encryption."""

from __future__ import annotations

from aioesphomeapi import InvalidEncryptionKeyAPIError
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

# The API key for noise encryption
NOISE_KEY = "N4Yle5YirwZhPiHHsdZLdOA73ndj/84veVaLhTvxCuU="


@pytest.mark.asyncio
async def test_host_mode_noise_encryption(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test Host mode with noise encryption enabled."""
    # Write, compile and run the ESPHome device, then connect to API
    # The API client should handle noise encryption automatically
    async with (
        run_compiled(yaml_config),
        api_client_connected(noise_psk=NOISE_KEY) as client,
    ):
        # If we can get device info, the encryption is working
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "host-noise-test"

        # List entities to ensure the encrypted connection is fully functional
        entities = await client.list_entities_services()
        assert entities is not None


@pytest.mark.asyncio
async def test_host_mode_noise_encryption_wrong_key(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test that connection fails with wrong encryption key."""
    # Write, compile and run the ESPHome device
    async with run_compiled(yaml_config):
        # Try to connect with wrong key - should fail with InvalidEncryptionKeyAPIError
        with pytest.raises(InvalidEncryptionKeyAPIError):
            async with api_client_connected(
                noise_psk="wrong_key_that_should_not_work",
                timeout=5,  # Shorter timeout for expected failure
            ) as client:
                # This should not be reached
                await client.device_info()
