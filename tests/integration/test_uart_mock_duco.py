"""Integration test for the Duco component against a model of the ventilation box.

Every entity reads its value from the box, a select or number change reaches the box, the clock
is sent and discovery reports the nodes. The CO2 answer holds an 0xAA byte and the status answer an
escaped AA 55, so the byte stuffing is exercised too. Most entities do not poll: their values come
from one burst of requests at boot, more than the hub could once keep open.
"""

from __future__ import annotations

import asyncio

from aioesphomeapi import (
    ButtonInfo,
    EntityState,
    NumberInfo,
    SelectInfo,
    SensorInfo,
    SwitchInfo,
    TextInfo,
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
        serial_node_2 = require_entity(entities, "serial_number_node_2", TextSensorInfo)
        version_node_3 = require_entity(entities, "version_node_3", TextSensorInfo)
        errors_node_1 = require_entity(entities, "errors_node_1", TextSensorInfo)
        for key, entity_type in (
            ("uptime_node_2", TextSensorInfo),
            ("node_type_2", TextSensorInfo),
            ("nightboost_time_start", TextInfo),
            ("nightboost_time_stop", TextInfo),
            ("mode_node_2", SelectInfo),
            ("bypass_control", SelectInfo),
            ("adaptive_bypass", SelectInfo),
            ("frost_heater", SelectInfo),
            ("passive_cooling", SelectInfo),
            ("nightboost", SelectInfo),
            ("nightboost_month_start", SelectInfo),
            ("nightboost_month_stop", SelectInfo),
            ("time_filter", SelectInfo),
            ("comfort_temperature_zone_1", NumberInfo),
            ("comfort_temperature_zone_2", NumberInfo),
            ("nightboost_temperature", NumberInfo),
            ("nightboost_max", NumberInfo),
            ("ventilation_min", NumberInfo),
            ("ventilation_max", NumberInfo),
            ("uptime_box_minutes", SensorInfo),
            ("dangerous_controls", SwitchInfo),
            ("installer_mode", SwitchInfo),
            ("locator_node_2", SwitchInfo),
            ("reboot", ButtonInfo),
        ):
            require_entity(entities, key, entity_type)

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
        await wait_for(lambda: state_of(serial_node_2) == "SN2234", "node 2 serial number")
        await wait_for(lambda: state_of(version_node_3) == "190.4.6.5", "node 3 version")
        await wait_for(
            lambda: state_of(errors_node_1) == "E31.19.01  Filters need replacement.\n",
            "node 1 errors",
        )
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
