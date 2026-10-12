"""Test the sensor rh_correction filter numerically."""

from __future__ import annotations

import asyncio

from aioesphomeapi import ButtonInfo, EntityState, SensorState
import pytest

from .state_utils import InitialStateHelper, build_key_to_entity_mapping
from .types import APIClientConnectedFactory, RunCompiledFunction

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
    loop = asyncio.get_running_loop()
    values: dict[str, list[float]] = {"rh_celsius": [], "rh_fahrenheit": []}
    done = loop.create_future()

    def on_state(state: EntityState) -> None:
        if not isinstance(state, SensorState) or state.missing_state:
            return
        name = key_to_sensor.get(state.key)
        if name not in values:
            return
        values[name].append(state.state)
        if all(len(v) == len(EXPECTED) for v in values.values()) and not done.done():
            done.set_result(True)

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):
        entities, _ = await client.list_entities_services()
        key_to_sensor = build_key_to_entity_mapping(entities, list(values))
        initial_state_helper = InitialStateHelper(entities)
        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        await initial_state_helper.wait_for_initial_states()

        button = next(
            e.key
            for e in entities
            if isinstance(e, ButtonInfo) and e.name == "Test RH Correction"
        )
        client.button_command(button)
        try:
            await asyncio.wait_for(done, timeout=2.0)
        except TimeoutError:
            pytest.fail(f"Timed out. Values: {values}")

        assert values["rh_celsius"] == EXPECTED
        # The same readings in °F give the same corrected humidity
        assert values["rh_fahrenheit"] == EXPECTED
