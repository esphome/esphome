"""Integration tests for the runtime-built use_address.

The default "<name>.local" address is no longer stored as a compile-time string;
it is built at runtime from the device name. This also fixes the logged address
when name_add_mac_suffix is enabled: the baked string used to miss the MAC
suffix, so it never matched the actual mDNS hostname.
"""

from __future__ import annotations

import asyncio

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

# Host platform default MAC: 98:35:69:ab:f6:79 -> suffix "abf679"
MAC_SUFFIX = "abf679"


@pytest.mark.asyncio
async def test_use_address_runtime(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """The API dump_config logs "<name>.local" built from the device name."""
    address_seen = asyncio.Event()

    def check_output(line: str) -> None:
        if "Address: use-address-runtime.local:" in line:
            address_seen.set()

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == "use-address-runtime"

        try:
            await asyncio.wait_for(address_seen.wait(), timeout=10.0)
        except TimeoutError:
            pytest.fail("Did not log 'Address: use-address-runtime.local:'")


@pytest.mark.asyncio
async def test_use_address_runtime_mac_suffix(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """With name_add_mac_suffix the logged address includes the MAC suffix."""
    address_seen = asyncio.Event()
    expected = f"Address: use-address-mac-{MAC_SUFFIX}.local:"

    def check_output(line: str) -> None:
        if expected in line:
            address_seen.set()

    async with (
        run_compiled(yaml_config, line_callback=check_output),
        api_client_connected() as client,
    ):
        device_info = await client.device_info()
        assert device_info is not None
        assert device_info.name == f"use-address-mac-{MAC_SUFFIX}"

        try:
            await asyncio.wait_for(address_seen.wait(), timeout=10.0)
        except TimeoutError:
            pytest.fail(f"Did not log '{expected}'")
