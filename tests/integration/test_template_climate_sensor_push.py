"""Integration test for template climate: current_temperature/current_humidity live sensor push.

A *later* change to a backing sensor's value -- not just its initial reading at boot -- propagates
into a new climate state via add_on_state_callback. Re-publishing the same sensor value again must
not cause a redundant climate state update.
"""

from __future__ import annotations

import asyncio
import math

import aioesphomeapi
from aioesphomeapi import ButtonInfo, ClimateInfo
import pytest

from .host_prefs import clear_host_prefs
from .state_utils import InitialStateHelper, require_entity, wait_for_state
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "tmpl-clim-sensor-push"


@pytest.mark.asyncio
async def test_template_climate_sensor_push(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """A later change to the backing sensor pushes a new climate state; an unchanged republish does not."""
    clear_host_prefs(DEVICE_NAME)

    state_updates: list[aioesphomeapi.ClimateState] = []

    async with (
        run_compiled(yaml_config),
        api_client_connected() as client,
    ):

        def on_state(state: aioesphomeapi.EntityState) -> None:
            if isinstance(state, aioesphomeapi.ClimateState):
                state_updates.append(state)

        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) == 1, "Expected exactly 1 climate entity"
        test_climate = climate_infos[0]

        publish_temp = require_entity(entities, "publish_temperature", ButtonInfo)
        publish_temp_same = require_entity(
            entities, "publish_temperature_same", ButtonInfo
        )
        publish_humidity = require_entity(entities, "publish_humidity", ButtonInfo)

        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        initial = initial_state_helper.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        # Neither backing sensor has published anything yet.
        assert math.isnan(initial.current_temperature)
        assert math.isnan(initial.current_humidity)

        # A later sensor reading -- not the initial one -- pushes a new climate state.
        client.button_command(publish_temp.key)
        state = await wait_for_state(
            client, lambda s: isinstance(s, aioesphomeapi.ClimateState)
        )
        assert state.current_temperature == pytest.approx(24.0, abs=0.1)

        client.button_command(publish_humidity.key)
        state = await wait_for_state(
            client, lambda s: isinstance(s, aioesphomeapi.ClimateState)
        )
        assert state.current_humidity == pytest.approx(65.0, abs=0.1)

        # Re-publishing the same temperature must not cause a redundant climate state update.
        updates_before = len(state_updates)
        client.button_command(publish_temp_same.key)
        await asyncio.sleep(0.3)
        assert len(state_updates) == updates_before, (
            "Re-publishing an unchanged sensor reading must not republish the climate state"
        )
