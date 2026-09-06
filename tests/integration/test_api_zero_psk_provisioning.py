"""Integration tests for provisioning the encryption key over a zero-PSK connection.

A device with `api: encryption:` but no key accepts Noise handshakes using the
well-known all-zeros PSK. The ephemeral X25519 exchange protects the key from
passive sniffing while it is provisioned; plaintext provisioning still works
but is deprecated.
"""

from __future__ import annotations

import asyncio
import base64

from aioesphomeapi import InvalidEncryptionKeyAPIError, RequiresEncryptionAPIError
import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

# The well-known provisioning PSK: base64 of 32 zero bytes
ZERO_PSK = base64.b64encode(bytes(32)).decode()
# A real key to provision
NEW_KEY = base64.b64encode(b"n" * 32)
# Time for the device to activate a newly saved key (100ms timer plus margin)
KEY_ACTIVATION_DELAY = 0.5


@pytest.fixture(autouse=True)
def isolated_preferences(monkeypatch: pytest.MonkeyPatch, tmp_path) -> None:
    """Keep host preferences per-test so every run starts unprovisioned."""
    monkeypatch.setenv("ESPHOME_PREFDIR", str(tmp_path / "prefs"))


@pytest.mark.asyncio
async def test_api_zero_psk_provisioning(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Exercise the reject paths, then provision a key over the zero-PSK channel."""
    async with run_compiled(yaml_config):
        # --- Pre-provisioning reject paths (device state is unchanged) ---

        # A wrong (non-zero) PSK fails against the zero provisioning PSK
        with pytest.raises(InvalidEncryptionKeyAPIError):
            async with api_client_connected(
                noise_psk=base64.b64encode(b"w" * 32).decode(), timeout=5
            ) as client:
                await client.device_info()

        # A plaintext client and a zero-PSK client can be connected at the
        # same time while the device is unprovisioned
        async with (
            api_client_connected() as plaintext_client,
            api_client_connected(noise_psk=ZERO_PSK) as noise_client,
        ):
            plaintext_info = await plaintext_client.device_info()
            noise_info = await noise_client.device_info()
            # Both transports advertise provisioning support so old and new
            # clients can decide how to provision
            assert plaintext_info.api_encryption_provisionable is True
            assert noise_info.api_encryption_provisionable is True

            # The all-zeros key is reserved as the provisioning PSK and is
            # rejected on both transports
            zero_key = base64.b64encode(bytes(32))
            assert await noise_client.noise_encryption_set_key(zero_key) is False
            assert await plaintext_client.noise_encryption_set_key(zero_key) is False

        # --- Provision over the zero-PSK channel ---

        # The unprovisioned device accepts the all-zeros PSK; the handshake's
        # ephemeral-ephemeral DH encrypts everything that follows
        async with api_client_connected(noise_psk=ZERO_PSK) as client:
            device_info = await client.device_info()
            assert device_info.name == "zero-psk-provision-test"
            assert device_info.api_encryption_supported is True
            assert device_info.api_encryption_provisionable is True

            assert await client.noise_encryption_set_key(NEW_KEY) is True

        # The device activates the new key shortly after responding
        await asyncio.sleep(KEY_ACTIVATION_DELAY)

        # The new key now works, and the device is no longer provisionable
        async with api_client_connected(noise_psk=NEW_KEY.decode()) as client:
            device_info = await client.device_info()
            assert device_info.name == "zero-psk-provision-test"
            assert device_info.api_encryption_provisionable is False

        # The zero PSK no longer works
        with pytest.raises(InvalidEncryptionKeyAPIError):
            async with api_client_connected(noise_psk=ZERO_PSK, timeout=5) as client:
                await client.device_info()

        # Plaintext no longer works
        with pytest.raises(RequiresEncryptionAPIError):
            async with api_client_connected(timeout=5) as client:
                await client.device_info()


@pytest.mark.asyncio
async def test_api_zero_psk_provisioning_plaintext(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The legacy plaintext provisioning path still works and warns."""
    log_lines: list[str] = []
    async with run_compiled(yaml_config, line_callback=log_lines.append):
        async with api_client_connected() as client:
            device_info = await client.device_info()
            assert device_info.name == "zero-psk-plaintext-test"

            assert await client.noise_encryption_set_key(NEW_KEY) is True

        await asyncio.sleep(KEY_ACTIVATION_DELAY)

        # The deprecation warning was logged
        assert any("deprecated" in line for line in log_lines)

        # The new key works; the zero PSK does not
        async with api_client_connected(noise_psk=NEW_KEY.decode()) as client:
            assert (await client.device_info()).name == "zero-psk-plaintext-test"

        with pytest.raises(InvalidEncryptionKeyAPIError):
            async with api_client_connected(noise_psk=ZERO_PSK, timeout=5) as client:
                await client.device_info()
