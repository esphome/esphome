"""The Uponor Smatrix hub prints each registered device's address in its config dump."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_uponor_smatrix_dump_devices(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each child gets one line in the hub's dump."""
    waiter = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await client.device_info()
        await waiter.wait_for_each("Device: 0x110BDE13", "Device: 0x110BDE14")
