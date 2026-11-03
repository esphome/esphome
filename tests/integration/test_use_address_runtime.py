"""Integration tests for the runtime-built use_address.

The default "<name>.local" address is no longer stored as a compile-time string;
it is built at runtime from the device name. This also fixes the logged address
when name_add_mac_suffix is enabled: the baked string used to miss the MAC
suffix, so it never matched the actual mDNS hostname.
"""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .state_utils import assert_device_name
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
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, "use-address-runtime")

        await waiter.wait_for("Address: use-address-runtime.local:", timeout=10.0)


@pytest.mark.asyncio
async def test_use_address_runtime_mac_suffix(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """With name_add_mac_suffix the logged address includes the MAC suffix."""
    waiter = LineWaiter()

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await assert_device_name(client, f"use-address-mac-{MAC_SUFFIX}")

        await waiter.wait_for(
            f"Address: use-address-mac-{MAC_SUFFIX}.local:", timeout=10.0
        )
