"""The VBus hub prints each listener's source, dest and command in its config dump."""

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
    """Each listener gets one line, with ANY for an unset filter."""
    waiter = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await client.device_info()
        await waiter.wait_for_each(
            "Listener: source 0x1122, dest 0x0010, command 0x0100",
            "Listener: source 0x1234, dest ANY, command 0x0100",
            "Listener: source ANY, dest 0x0010, command ANY",
        )
