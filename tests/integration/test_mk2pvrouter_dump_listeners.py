"""The Mk2PVRouter hub prints each listener's config dump."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_mk2pvrouter_dump_listeners(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each listener prints its entity and tag, as when it was a component."""
    waiter = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await client.device_info()
        await waiter.wait_for_each(
            "Mk2PVRouter Sensor 'Power'",
            "Tag: P",
            "Mk2PVRouter Sensor 'Voltage Phase 1'",
            "Tag: V1",
            "Mk2PVRouter Binary Sensor 'Relay 2'",
            "Tag: R2",
            "Mk2PVRouter Text Sensor 'S per MC'",
            "Tag: S_MC",
        )
