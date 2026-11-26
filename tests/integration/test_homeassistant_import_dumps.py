"""The API server prints the config dump of every imported Home Assistant entity."""

from __future__ import annotations

import pytest

from .log_utils import LineWaiter
from .types import APIClientConnectedFactory, RunCompiledFunction


@pytest.mark.asyncio
async def test_homeassistant_import_dumps(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Each import prints the lines it printed when it was a component."""
    waiter = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await client.device_info()
        await waiter.wait_for_each(
            "Homeassistant Sensor 'Outside Temperature'",
            "Entity ID: 'sensor.outside'",
            "Attribute: 'temperature'",
            "Homeassistant Binary Sensor 'Motion'",
            "Entity ID: 'binary_sensor.motion'",
            "Homeassistant Text Sensor 'Status'",
            "Entity ID: 'sensor.status'",
            "Entity ID: 'switch.relay'",
            "Entity ID: 'select.mode'",
            "Homeassistant Number 'Target'",
            "Entity ID: 'number.target'",
            "Homeassistant Text 'Note'",
            "Entity ID: 'text.note'",
        )
