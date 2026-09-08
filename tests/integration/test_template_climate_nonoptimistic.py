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
from .state_utils import InitialStateHelper, require_entity, wait_for_state
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

    log_lines: list[str] = []
    state_updates: list[aioesphomeapi.ClimateState] = []

    def on_log_line(line: str) -> None:
        if "on_control " in line:
            log_lines.append(line)

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
        test_climate = climate_infos[0]

        confirm_button = require_entity(
            entities, "simulate_device_confirmation", ButtonInfo
        )

        client.subscribe_states(initial_state_helper.on_state_wrapper(on_state))
        try:
            await initial_state_helper.wait_for_initial_states()
        except TimeoutError:
            pytest.fail("Timeout waiting for initial states")

        initial = initial_state_helper.initial_states.get(test_climate.key)
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
        assert any(
            "on_control mode=3" in line for line in log_lines
        )  # CLIMATE_MODE_HEAT
        assert any("on_control target_temperature=22.5" in line for line in log_lines)
        assert any("on_control fan_mode=" in line for line in log_lines)
        assert any("on_control swing_mode=" in line for line in log_lines)
        assert any("on_control preset=" in line for line in log_lines)
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
