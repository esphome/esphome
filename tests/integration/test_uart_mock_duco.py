"""Integration test for the Duco component against a model of the ventilation box.

Every entity reads its value from the box, a select or number change reaches the box, the clock
is sent and discovery reports the nodes. The CO2 answer holds an 0xAA byte and the status answer an
escaped AA 55, so the byte stuffing is exercised too. Most entities do not poll: their values come
from one burst of requests at boot, more than the hub could once keep open.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    EntityState,
    NumberInfo,
    SelectInfo,
    SensorInfo,
    TextSensorInfo,
)
import pytest

from .log_utils import LineWaiter
from .state_utils import require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction

EXPECTED_SENSORS = {
    "co2": 682.0,
    "temperature": 21.5,
    "humidity": 54.32,
    "filter_remaining": 120.0,
    "flow_level": 45.0,
    "time_remaining": 21930.0,
    "bypass": 50.0,
    "temperature_oda": -3.5,
    "temperature_sup": 19.5,
    "temperature_eta": 22.0,
    "temperature_eha": 8.0,
}


@pytest.mark.asyncio
async def test_uart_mock_duco(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """All entities read the box; mode and comfort temperature changes are written."""
    waiter = LineWaiter()
    states: dict[int, EntityState] = {}

    async def wait_for(condition, what: str) -> None:
        try:
            async with asyncio.timeout(10.0):
                while not condition():
                    await asyncio.sleep(0.05)
        except TimeoutError:
            pytest.fail(f"Timeout waiting for {what}; states: {states}")

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        sensors = {
            key: require_entity(entities, key, SensorInfo, exact=True)
            for key in EXPECTED_SENSORS
        }
        mode = require_entity(entities, "mode", SelectInfo)
        comfort = require_entity(entities, "comfort_temperature", NumberInfo)
        serial = require_entity(entities, "serial_number", TextSensorInfo)

        client.subscribe_states(lambda state: states.__setitem__(state.key, state))

        def state_of(entity):
            state = states.get(entity.key)
            return None if state is None or state.missing_state else state.state

        for key, expected in EXPECTED_SENSORS.items():
            await wait_for(
                lambda e=sensors[key], v=expected: state_of(e) == pytest.approx(v),
                key,
            )
        await wait_for(lambda: state_of(mode) == "MAN1", "mode")
        await wait_for(lambda: state_of(comfort) == pytest.approx(21.0), "comfort")
        await wait_for(lambda: state_of(serial) == "SN1234", "serial number")
        await waiter.wait_for("Time set to")

        client.select_command(mode.key, "CNT2")
        await waiter.wait_for("Mode set to 09")
        await wait_for(lambda: state_of(mode) == "CNT2", "new mode")

        client.number_command(comfort.key, 22.3)
        await waiter.wait_for("Comfort set to 223")
        await wait_for(lambda: state_of(comfort) == pytest.approx(22.3), "new comfort")

        await waiter.wait_for_each(
            "Found node 1: type 17", "Found node 3: type 12", timeout=40.0
        )
