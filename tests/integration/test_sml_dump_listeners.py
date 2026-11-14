"""The SML hub prints each listener's config dump."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_sml_dump_listeners(
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
            "SML:",
            "SML 'Total Energy'",
            "Server ID: 0123456789abcdef",
            "OBIS Code: 1-0:1.8.0",
            "SML 'Active Power'",
            "OBIS Code: 1-0:16.7.0",
            "SML 'Manufacturer'",
            "OBIS Code: 129-129:199.130.3",
        )
