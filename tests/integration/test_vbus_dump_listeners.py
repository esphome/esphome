"""The VBus hub prints each listener's config dump."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_vbus_dump_listeners(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each listener prints its model and entities, as when it was a component."""
    waiter = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await client.device_info()
        await waiter.wait_for_each(
            "Deltasol CS4:",
            "Temperature 1 'CS4 Temp 1'",
            "VBus Custom Sensor:",
            "Source address: 0x1234",
            "Dest address: ANY",
            "Command: 0x0100",
            "- 'Custom Value'",
            "VBus Custom Binary Sensor:",
            "Source address: ANY",
            "Dest address: 0x0010",
            "- 'Custom Flag'",
        )
