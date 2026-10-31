"""The Uponor Smatrix hub prints each sensor device's config dump."""

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
    """Sensors print through the hub; the climate still prints its own dump."""
    waiter = LineWaiter()
    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        await client.device_info()
        await waiter.wait_for_each(
            "Uponor Smatrix Sensor",
            "Device address: 0x110BDE13",
            "Temperature 'Temperature Living Room'",
            "Device address: 0x110BDE14",
            "Humidity 'Humidity Kitchen'",
            "Uponor Smatrix Climate 'Thermostat Bedroom'",
            "Device address: 0x110BDE15",
        )
