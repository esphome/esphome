"""Integration test for Host mode reconnection."""

from __future__ import annotations

import pytest

from .state_utils import assert_device_name
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_reconnect(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test reconnecting to a Host mode device."""
    # Write, compile and run the ESPHome device
    async with run_compiled(yaml_config):
        # First connection
        async with api_client_connected() as client:
            device_info = await client.device_info()
            assert device_info is not None

        # Reconnect with a new client
        async with api_client_connected() as client2:
            await assert_device_name(client2, device_info.name)
