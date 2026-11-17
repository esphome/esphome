"""Integration test for template climate: optimistic: false.

A command still fires on_control (so a real device-backed config can forward it out), but must
NOT change the entity's own state -- only an explicit climate.template.publish call (standing in
for the device confirming the command actually took effect) does that.
"""

from __future__ import annotations

import asyncio

import aioesphomeapi
from aioesphomeapi import (
    ButtonInfo,
    ClimateFanMode,
    ClimateInfo,
    ClimateMode,
    ClimatePreset,
    ClimateSwingMode,
)
import pytest

from .host_prefs import clear_host_prefs
from .log_utils import LineWaiter
from .state_utils import (
    require_entity,
    require_only_entity,
    subscribe_after_initial_states,
    wait_for_state,
)
from .types import APIClientConnectedFactory, RunCompiledFunction

DEVICE_NAME = "tmpl-clim-nonopt"


@pytest.mark.asyncio
async def test_template_climate_nonoptimistic(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
) -> None:
    """Nonoptimistic: a command doesn't change state until explicitly published."""
    clear_host_prefs(DEVICE_NAME)

    waiter = LineWaiter()
    state_updates: list[aioesphomeapi.ClimateState] = []

    async with (
        run_compiled(yaml_config, line_callback=waiter.callback),
        api_client_connected() as client,
    ):

        def on_state(state: aioesphomeapi.EntityState) -> None:
            if isinstance(state, aioesphomeapi.ClimateState):
                state_updates.append(state)

        entities, _ = await client.list_entities_services()
        test_climate = require_only_entity(entities, ClimateInfo)

        confirm_button = require_entity(
            entities, "simulate_device_confirmation", ButtonInfo
        )

        sub = await subscribe_after_initial_states(client, entities, on_state=on_state)

        initial = sub.initial_states.get(test_climate.key)
        assert initial is not None, "No initial climate state received"
        assert isinstance(initial, aioesphomeapi.ClimateState)
        assert initial.mode == ClimateMode.OFF

        # Send every settable field in one command. on_control must fire with all of them, but
        # nothing may be applied to the entity's own state -- no ClimateState update at all.
        client.climate_command(
            test_climate.key,
            mode=ClimateMode.HEAT,
            target_temperature=22.5,
            fan_mode=ClimateFanMode.HIGH,
            swing_mode=ClimateSwingMode.VERTICAL,
            preset=ClimatePreset.AWAY,
        )
        await asyncio.sleep(0.3)
        await waiter.wait_for_each(
            "on_control mode=3",  # CLIMATE_MODE_HEAT
            "on_control target_temperature=22.5",
            "on_control fan_mode=",
            "on_control swing_mode=",
            "on_control preset=",
            timeout=5.0,
        )
        assert not state_updates, (
            "optimistic: false must not publish a state until climate.template.publish reports it"
        )

        # The device confirms the command actually took effect.
        client.button_command(confirm_button.key)
        state = await wait_for_state(
            client, lambda s: isinstance(s, aioesphomeapi.ClimateState)
        )
        assert state.mode == ClimateMode.HEAT
        assert state.target_temperature == pytest.approx(22.5, abs=0.1)
        assert state.fan_mode == ClimateFanMode.HIGH
        assert state.swing_mode == ClimateSwingMode.VERTICAL
        assert state.preset == ClimatePreset.AWAY
