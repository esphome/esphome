"""Integration test for template climate: climate.template.publish covering every field at once.

A single climate.template.publish call resolves into exactly one ClimateState update, and never
triggers on_control (which would misrepresent a device state report as a fresh command). This also
exercises that a sensor/humidity_sensor whose reading matches what's about to be published doesn't
sneak in an extra state update of its own (the sensor callback only re-publishes on an actual
change).
"""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import (
    ButtonInfo,
    ClimateAction,
    ClimateFanMode,
    ClimateInfo,
    ClimateMode,
    ClimatePreset,
    ClimateSwingMode,
)
import pytest

from .host_prefs import clear_host_prefs
from .state_utils import InitialStateHelper, require_entity, wait_for_state
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "tmpl-clim-publish-all"


@pytest.mark.asyncio
async def test_template_climate_publish_all_fields(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """One climate.template.publish call setting every field resolves to one state update."""
    clear_host_prefs(DEVICE_NAME)

    state_updates: list[aioesphomeapi.ClimateState] = []
    on_control_count = 0

    def on_log_line(line: str) -> None:
        nonlocal on_control_count
        if "on_control fired" in line:
            on_control_count += 1

    async with (
        run_compiled(yaml_config, line_callback=on_log_line),
        api_client_connected() as client,
    ):

        def on_state(state: aioesphomeapi.EntityState) -> None:
            if isinstance(state, aioesphomeapi.ClimateState):
                state_updates.append(state)

        entities, _ = await client.list_entities_services()
        initial_state_helper = InitialStateHelper(entities)
        climate_infos = [e for e in entities if isinstance(e, ClimateInfo)]
        assert len(climate_infos) == 1, "Expected exactly 1 climate entity"

        publish_button = require_entity(entities, "publish_all", ButtonInfo)

        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        client.button_command(publish_button.key)
        try:
            state = await wait_for_state(
                client, lambda s: isinstance(s, aioesphomeapi.ClimateState)
            )
        except TimeoutError:
            pytest.fail("Timeout waiting for the published climate state")

        assert state.current_temperature == pytest.approx(20.0, abs=0.1)
        assert state.current_humidity == pytest.approx(60.0, abs=0.1)
        assert state.target_temperature == pytest.approx(23.0, abs=0.1)
        assert state.mode == ClimateMode.HEAT
        assert state.action == ClimateAction.HEATING
        assert state.fan_mode == ClimateFanMode.HIGH
        assert state.swing_mode == ClimateSwingMode.VERTICAL
        assert state.preset == ClimatePreset.ECO

        # Give any stray extra update (there shouldn't be one) a moment to arrive.
        await asyncio.sleep(0.2)
        assert len(state_updates) == 1, (
            f"Expected exactly one ClimateState update, got {len(state_updates)}"
        )
        assert on_control_count == 0, (
            "climate.template.publish must not trigger on_control"
        )
