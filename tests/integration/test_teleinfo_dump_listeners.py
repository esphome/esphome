"""The TeleInfo hub prints each listener's config dump."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_teleinfo_dump_listeners(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each listener prints the lines it printed when it was a component."""
    waiter = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await client.device_info()
        await waiter.wait_for_each(
            "TeleInfo:",
            "Teleinfo Sensor 'Energy Index'",
            "Teleinfo Sensor 'Current'",
            "Teleinfo Text Sensor 'Meter Address'",
        )
