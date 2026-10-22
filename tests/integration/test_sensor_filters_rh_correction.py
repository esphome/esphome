"""Test the sensor rh_correction filter numerically."""

from __future__ import annotations

from aioesphomeapi import ButtonInfo
import pytest

from .state_utils import InitialStateHelper, SensorStateCollector, require_entity
from .types import APIClientConnectedFactory, RunCompiledFunction
from .wait_utils import wait_or_fail

# 40 % at 25 °C, measured 7.3 °C warmer, in single precision Magnus arithmetic
CORRECTED = pytest.approx(61.13, abs=0.05)
# Corrected, clamped, passed through out of band, corrected again; the NaN step is dropped
EXPECTED = [CORRECTED, 100.0, 40.0, CORRECTED]


@pytest.mark.asyncio
async def test_sensor_filters_rh_correction(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    collector = SensorStateCollector(sensor_names=["rh_celsius", "rh_fahrenheit"])

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        collector.build_key_mapping(entities)
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(
            initial_state_helper.on_state_wrapper(collector.on_state)
        )
        await initial_state_helper.wait_for_initial_states()

        done = collector.add_waiter(
            lambda: all(
                len(values) == len(EXPECTED)
                for values in collector.sensor_states.values()
            )
        )
        button = require_entity(entities, "test_rh_correction", ButtonInfo, exact=True)
        client.button_command(button.key)
        await wait_or_fail(
            done,
            2.0,
            "Timed out waiting for the corrected humidity values",
            values=collector.sensor_states,
        )

        assert collector.sensor_states["rh_celsius"] == EXPECTED
        # The same readings in °F give the same corrected humidity
        assert collector.sensor_states["rh_fahrenheit"] == EXPECTED
