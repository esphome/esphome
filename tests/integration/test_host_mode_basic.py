"""Basic integration test for Host mode."""

from __future__ import annotations

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_host_mode_basic(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Test basic Host mode functionality with API connection."""
    # Write, compile and run the ESPHome device, then connect to API
    async with run_compiled(yaml_config), api_client_connected() as client:
        # Verify we can get device info
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "host-test"
