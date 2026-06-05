"""Regression test for issue #16798: nullptr deref during API client teardown.

When an API client is removed, ``APIServer::remove_client_`` resets the client
slot (running ``~APIConnection()``) before decrementing the active client count.
If anything publishes entity state from inside that destructor -- on real
hardware, voice_assistant unsubscribes there and its ``on_client_disconnected``
automation drives a light -- ``APIServer::on_light_update`` iterates the active
clients and dereferences the half-removed (null) slot, crashing the device.

The ``api_connection_destroy_light`` test component installs a hook that publishes
a light from inside ``~APIConnection()``, reproducing the exact reentrancy without
needing voice_assistant (which can't build on host). Before the fix the host
process crashes when the first client disconnects; after the fix it survives and
a second client can reconnect.
"""

from __future__ import annotations

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_api_disconnect_reentrant_light(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A client disconnect that publishes state mid-teardown must not crash."""
    async with run_compiled(yaml_config):
        # First connection: disconnecting it triggers remove_client_ ->
        # ~APIConnection() -> light publish -> on_light_update over the
        # mid-removal slot. Pre-fix this crashes the host process.
        async with api_client_connected() as client:
            device_info = await client.device_info()
            assert device_info is not None

        # If the device survived the reentrant publish above, a fresh client can
        # still connect. Pre-fix the process is dead and this connection fails.
        async with api_client_connected() as client2:
            device_info2 = await client2.device_info()
            assert device_info2 is not None
            assert device_info2.name == device_info.name
